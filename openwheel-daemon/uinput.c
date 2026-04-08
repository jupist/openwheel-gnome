// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors

#include "uinput.h"
#include "keymap.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <string.h>
#include <syslog.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

static int g_fd = -1;

static int emit(int type, int code, int value)
{
    if (g_fd < 0) return -1;
    struct input_event ie;
    memset(&ie, 0, sizeof(ie));
    ie.type = type;
    ie.code = code;
    ie.value = value;
    // Kernel ignores ie.time on writes — leaving zeroed is fine.
    if (write(g_fd, &ie, sizeof(ie)) != (ssize_t)sizeof(ie)) {
        syslog(LOG_WARNING, "uinput write failed: %s", strerror(errno));
        return -1;
    }
    return 0;
}

static int sync_report(void)
{
    return emit(EV_SYN, SYN_REPORT, 0);
}

int uinput_init(void)
{
    if (g_fd >= 0) return 0;

    g_fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (g_fd < 0) {
        syslog(LOG_WARNING,
               "uinput: open(/dev/uinput) failed: %s — keystroke/scroll injection disabled",
               strerror(errno));
        return -1;
    }

    // Enable EV_KEY and register every key code from the keymap (so we
    // never have to call UI_SET_KEYBIT later, even for codes that aren't
    // in the keymap yet — we additionally register the modifiers and
    // mouse buttons explicitly below).
    if (ioctl(g_fd, UI_SET_EVBIT, EV_KEY) < 0) goto fail;

    for (size_t i = 0; i < keymap_count(); ++i) {
        int code = keymap_code_at(i);
        if (code >= 0) {
            ioctl(g_fd, UI_SET_KEYBIT, code);
        }
    }

    // Modifiers (some are also in the keymap but make sure they're set).
    int mods[] = {
        KEY_LEFTCTRL, KEY_RIGHTCTRL,
        KEY_LEFTSHIFT, KEY_RIGHTSHIFT,
        KEY_LEFTALT, KEY_RIGHTALT,
        KEY_LEFTMETA, KEY_RIGHTMETA,
    };
    for (size_t i = 0; i < sizeof(mods) / sizeof(mods[0]); ++i) {
        ioctl(g_fd, UI_SET_KEYBIT, mods[i]);
    }

    // Mouse buttons.
    int btns[] = { BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, BTN_SIDE, BTN_EXTRA };
    for (size_t i = 0; i < sizeof(btns) / sizeof(btns[0]); ++i) {
        ioctl(g_fd, UI_SET_KEYBIT, btns[i]);
    }

    // Relative axes for scroll wheels.
    if (ioctl(g_fd, UI_SET_EVBIT, EV_REL) < 0) goto fail;
    ioctl(g_fd, UI_SET_RELBIT, REL_WHEEL);
    ioctl(g_fd, UI_SET_RELBIT, REL_HWHEEL);
    // High-resolution wheels (kernel 5.0+) — many compositors expect these.
    ioctl(g_fd, UI_SET_RELBIT, REL_WHEEL_HI_RES);
    ioctl(g_fd, UI_SET_RELBIT, REL_HWHEEL_HI_RES);

    // Create the device.
    struct uinput_setup usetup;
    memset(&usetup, 0, sizeof(usetup));
    usetup.id.bustype = BUS_VIRTUAL;
    usetup.id.vendor  = 0x0b05;  // ASUSTek (cosmetic)
    usetup.id.product = 0x1989;  // arbitrary
    usetup.id.version = 1;
    strncpy(usetup.name, "OpenWheel Virtual Input", sizeof(usetup.name) - 1);

    if (ioctl(g_fd, UI_DEV_SETUP, &usetup) < 0) goto fail;
    if (ioctl(g_fd, UI_DEV_CREATE) < 0) goto fail;

    // Give the kernel/compositor a moment to notice the new device before
    // we start sending events. Without this, the very first injected key
    // can be lost on some compositors.
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 200 * 1000 * 1000 };
    nanosleep(&ts, NULL);

    syslog(LOG_INFO, "uinput: virtual input device created");
    return 0;

fail:
    syslog(LOG_WARNING, "uinput: device setup failed: %s", strerror(errno));
    close(g_fd);
    g_fd = -1;
    return -1;
}

void uinput_cleanup(void)
{
    if (g_fd < 0) return;
    ioctl(g_fd, UI_DEV_DESTROY);
    close(g_fd);
    g_fd = -1;
}

int uinput_available(void)
{
    return g_fd >= 0;
}

static void press_modifiers(uint32_t modifiers, int press)
{
    if (modifiers & OW_MOD_CTRL)  emit(EV_KEY, KEY_LEFTCTRL,  press);
    if (modifiers & OW_MOD_SHIFT) emit(EV_KEY, KEY_LEFTSHIFT, press);
    if (modifiers & OW_MOD_ALT)   emit(EV_KEY, KEY_LEFTALT,   press);
    if (modifiers & OW_MOD_SUPER) emit(EV_KEY, KEY_LEFTMETA,  press);
}

int uinput_tap_key(int code, uint32_t modifiers)
{
    if (g_fd < 0) return -1;
    if (code <= 0) return -1;

    press_modifiers(modifiers, 1);
    emit(EV_KEY, code, 1);
    sync_report();

    emit(EV_KEY, code, 0);
    press_modifiers(modifiers, 0);
    sync_report();
    return 0;
}

int uinput_scroll_vertical(int clicks)
{
    if (g_fd < 0) return -1;
    if (clicks == 0) return 0;
    // High-res value: 120 units per detent, matches kernel input convention.
    int hi = clicks * 120;
    emit(EV_REL, REL_WHEEL_HI_RES, hi);
    emit(EV_REL, REL_WHEEL, clicks);
    sync_report();
    return 0;
}

int uinput_scroll_horizontal(int clicks)
{
    if (g_fd < 0) return -1;
    if (clicks == 0) return 0;
    int hi = clicks * 120;
    emit(EV_REL, REL_HWHEEL_HI_RES, hi);
    emit(EV_REL, REL_HWHEEL, clicks);
    sync_report();
    return 0;
}

int uinput_tap_button(int btn_code)
{
    if (g_fd < 0) return -1;
    emit(EV_KEY, btn_code, 1);
    sync_report();
    emit(EV_KEY, btn_code, 0);
    sync_report();
    return 0;
}

int uinput_hold_modifiers(uint32_t modifiers)
{
    if (g_fd < 0) return -1;
    press_modifiers(modifiers, 1);
    sync_report();
    return 0;
}

int uinput_release_modifiers(uint32_t modifiers)
{
    if (g_fd < 0) return -1;
    press_modifiers(modifiers, 0);
    sync_report();
    return 0;
}
