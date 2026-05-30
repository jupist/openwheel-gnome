#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <dbus/dbus.h>
#include <sys/syslog.h>

#include "openwheel.h"
#include "helpers.h"
#include "dbus_methods.h"
#include "uinput.h"

static volatile sig_atomic_t g_should_exit = 0;

static void on_term(int sig)
{
    (void)sig;
    g_should_exit = 1;
}

// If DBUS_SESSION_BUS_ADDRESS is not in the environment (common when the
// daemon is started via sudo, which strips most env vars), scan
// /run/user/*/bus for the first active session socket and set the variable
// so that dbus_bus_get(DBUS_BUS_SESSION, ...) can find it.
static void autodetect_session_bus(void)
{
    if (getenv("DBUS_SESSION_BUS_ADDRESS")) return;

    DIR *d = opendir("/run/user");
    if (!d) return;

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char sock[256];
        snprintf(sock, sizeof(sock), "/run/user/%s/bus", entry->d_name);
        if (access(sock, F_OK) == 0) {
            char addr[280];
            snprintf(addr, sizeof(addr), "unix:path=/run/user/%s/bus", entry->d_name);
            setenv("DBUS_SESSION_BUS_ADDRESS", addr, 1);
            syslog(LOG_INFO, "session bus auto-detected: %s", addr);
            break;
        }
    }
    closedir(d);
}

int main() {
    openlog("openwheel", LOG_PID, LOG_DAEMON);
    autodetect_session_bus();

    // Clean shutdown so the uinput device is destroyed.
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_term;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    char device_path[BUFFER_SIZE];
    if (find_hidraw_device(device_path, sizeof(device_path)) < 0) {
        syslog(LOG_ERR, "ASUS Dial device not found");
        fprintf(stderr, "FATAL: ASUS Dial device not found (no ASUS2020 in /sys/class/hidraw)\n");
        return 1;
    }

    syslog(LOG_INFO, "Found ASUS Dial at %s", device_path);
    fprintf(stderr, "INFO: Found dial at %s\n", device_path);

    int fd = open(device_path, O_RDONLY);
    if (fd < 0) {
        syslog(LOG_ERR, "Failed to open HID device %s", device_path);
        fprintf(stderr, "FATAL: Cannot open %s: %s\n", device_path, strerror(errno));
        return 1;
    }

    // Set up D-Bus connection
    const char *bus_addr = getenv("DBUS_SESSION_BUS_ADDRESS");
    fprintf(stderr, "INFO: DBUS_SESSION_BUS_ADDRESS=%s\n", bus_addr ? bus_addr : "(not set)");

    DBusConnection* connection;
    DBusError err;

    dbus_error_init(&err);
    connection = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (dbus_error_is_set(&err)) {
        syslog(LOG_ERR, "D-Bus connection error (%s)", err.message);
        fprintf(stderr, "FATAL: D-Bus connect failed: %s\n", err.message);
        dbus_error_free(&err);
    }
    if (connection == NULL) {
        fprintf(stderr, "FATAL: D-Bus connection is NULL\n");
        return 1;
    }

    // Request the name 'org.asus.dial'
    int ret = dbus_bus_request_name(connection, "org.asus.dial", DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (dbus_error_is_set(&err)) {
        syslog(LOG_ERR, "Failed to request D-Bus name 'org.asus.dial' (%s)", err.message);
        fprintf(stderr, "FATAL: D-Bus name request failed: %s\n", err.message);
        dbus_error_free(&err);
    }
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        syslog(LOG_ERR, "Failed to become primary owner of 'org.asus.dial' (ret=%d)", ret);
        fprintf(stderr, "FATAL: Not primary owner of org.asus.dial (ret=%d — another instance running?)\n", ret);
        return 1;
    }
    fprintf(stderr, "INFO: D-Bus name acquired, uinput next\n");

    // Bring up the virtual input device. Failure here is non-fatal — the
    // daemon still emits Rotate/Press signals, just without injection.
    if (uinput_init() != 0) {
        syslog(LOG_WARNING,
               "Continuing without uinput. Wayland keystroke/scroll injection "
               "will be unavailable. Check that /dev/uinput exists and the "
               "user running the daemon has access (see 99-openwheel.rules).");
    }

    // Register the org.asus.dial method handlers (InjectKey, InjectScroll, ...).
    if (dbus_methods_register(connection) != 0) {
        syslog(LOG_ERR, "Failed to register D-Bus method handlers");
    }

    // Get the unix fd that libdbus is using so we can poll() it alongside
    // the HID device. This is the standard way to integrate libdbus into
    // an existing event loop without spawning a thread.
    int dbus_fd = -1;
    if (!dbus_connection_get_unix_fd(connection, &dbus_fd)) {
        syslog(LOG_WARNING, "dbus_connection_get_unix_fd failed; method calls will not be served");
        dbus_fd = -1;
    }

    WheelPacket pkt;
    int was_down = 0;

    send_notification("OpenWheel", "OpenWheel openwheel-daemon started");

    struct pollfd fds[2];
    fds[0].fd = fd;
    fds[0].events = POLLIN;
    fds[1].fd = dbus_fd;
    fds[1].events = POLLIN;

    nfds_t nfds = (dbus_fd >= 0) ? 2 : 1;

    while (!g_should_exit) {
        // Use a 1s timeout so signals can interrupt poll quickly enough.
        int pr = poll(fds, nfds, 1000);
        if (pr < 0) {
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "poll failed: %s", strerror(errno));
            break;
        }

        // Always service D-Bus first so replies go out promptly.
        if (dbus_fd >= 0) {
            dbus_methods_dispatch(connection);
        }

        if (fds[0].revents & POLLIN) {
            int bytesRead = read(fd, &pkt, sizeof(pkt));
            if (bytesRead < 0) {
                if (errno == EINTR) continue;
                syslog(LOG_ERR, "Failed to read from HID device: %s", strerror(errno));
                break;
            }
            if (bytesRead == 4) {
                if (pkt.rotation_hb == ROTATE_PLUS) {
                    send_dbus_signal(connection, DIAL_ROTATE_SIGNAL, 1);
                    syslog(LOG_DEBUG, "sent rotate plus");
                }

                if (pkt.rotation_hb == ROTATE_MINUS) {
                    send_dbus_signal(connection, DIAL_ROTATE_SIGNAL, -1);
                    syslog(LOG_DEBUG, "sent rotate minus");
                }

                if (pkt.button == BUTTON_DOWN) {
                    send_dbus_signal(connection, DIAL_PRESS_SIGNAL, 1);
                    was_down = 1;
                    syslog(LOG_DEBUG, "sent button down");
                }

                if (pkt.button == BUTTON_UP && was_down) {
                    send_dbus_signal(connection, DIAL_PRESS_SIGNAL, 0);
                    was_down = 0;
                    syslog(LOG_DEBUG, "sent button up");
                }
            } else if (bytesRead > 0) {
                syslog(LOG_ERR, "Malformed packet received (%d bytes)", bytesRead);
            }
        }
    }

    syslog(LOG_INFO, "Shutting down");
    uinput_cleanup();
    close(fd);
    return 0;
}
