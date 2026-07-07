e

# OpenWheel — Build & Install Guide

## Dependencies

### Required

| Package                    | Fedora                      | Ubuntu/Debian           | Arch                |
| -------------------------- | --------------------------- | ----------------------- | ------------------- |
| CMake 3.16+                | `cmake`                   | `cmake`               | `cmake`           |
| C/C++20 compiler           | `gcc-c++`                 | `build-essential`     | `base-devel`      |
| pkg-config                 | `pkgconf`                 | `pkg-config`          | `pkgconf`         |
| libdbus-1                  | `dbus-devel`              | `libdbus-1-dev`       | `dbus`            |
| Qt 6 Core/Gui/Widgets/DBus | `qt6-qtbase-devel`        | `qt6-base-dev`        | `qt6-base`        |
| Qt 6 Qml/Quick/Controls2   | `qt6-qtdeclarative-devel` | `qt6-declarative-dev` | `qt6-declarative` |

### Optional

| Package                                                                                                            | Purpose                                |
| ------------------------------------------------------------------------------------------------------------------ | -------------------------------------- |
| `libX11-devel` + `libXtst-devel`                                                                               | X11 XTest fallback (X11 sessions only) |
| KDE Frameworks 6 (`extra-cmake-modules`, `libkf6coreaddons-dev`, `libkf6dbusaddons-dev`, `libkf6i18n-dev`) | Single-instance guard on KDE           |

## Building

```bash
# Configure (from repo root)
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build everything
cmake --build build -j$(nproc)

# Run unit tests
cd build && ctest --output-on-failure && cd ..
```

### Build options

| Option               | Default | Description                      |
| -------------------- | ------- | -------------------------------- |
| `BUILD_DAEMON`     | ON      | Build`asus_wheel` daemon       |
| `BUILD_GADGET`     | ON      | Build`openwheel-gadget` Qt app |
| `BUILD_TESTS`      | ON      | Build CTest suite                |
| `CMAKE_BUILD_TYPE` | Release | Debug / Release / RelWithDebInfo |

### Faster builds with Ninja

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Installing

Install to `/usr` so that udev rules and systemd unit end up in the standard locations udev and systemd scan by default:

```bash
sudo cmake --install build --prefix /usr
```

What gets installed:

| Path                                                 | Contents                    |
| ---------------------------------------------------- | --------------------------- |
| `/usr/bin/asus_wheel`                              | Daemon executable           |
| `/usr/bin/openwheel-gadget`                        | Gadget executable           |
| `/usr/share/openwheel/profiles/`                   | Bundled JSON profiles       |
| `/usr/share/applications/openwheel-gadget.desktop` | App menu entry              |
| `/etc/xdg/autostart/openwheel-gadget.desktop`      | Gadget autostart on login   |
| `/usr/lib/udev/rules.d/99-openwheel.rules`         | Device access rules         |
| `/usr/lib/systemd/user/openwheel-daemon.service`   | Daemon systemd user service |

### Local install (no root)

```bash
cmake --install build --prefix ~/.local
# udev and systemd files need to be copied manually:
sudo cp udev/99-openwheel.rules /etc/udev/rules.d/
sudo udevadm control --reload && sudo udevadm trigger
mkdir -p ~/.config/systemd/user
cp build/openwheel-daemon/openwheel-daemon.service ~/.config/systemd/user/
# Edit ExecStart in the service file to point to ~/.local/bin/asus_wheel
```

## Device access (one-time setup)

The daemon needs read access to the ASUS Dial (`/dev/hidraw*`) and write access to `/dev/uinput` for keystroke injection.

```bash
# 1. Apply udev rules (already done if you installed with --prefix /usr)
sudo udevadm control --reload
sudo udevadm trigger

# 2. Add your user to the input group
sudo usermod -aG input $USER

# 3. Log out and back in for the group change to take effect
```

Verify access:

```bash
groups | grep input       # should list 'input'
ls -l /dev/uinput         # should show group 'input' with rw permission
```

## Enabling autostart

```bash
# Enable the daemon as a systemd user service
systemctl --user enable --now openwheel-daemon

# Check status
systemctl --user status openwheel-daemon

# View daemon logs
journalctl --user -u openwheel-daemon -f
```

The gadget autostarts via `/etc/xdg/autostart/openwheel-gadget.desktop` on next login. To start it now without logging out:

```bash
openwheel-gadget &
```

## Smoke tests (manual, not in ctest)

These tests need real hardware or a running daemon:

```bash
# Build smoke tests
cmake --build build --target smoke_brightness smoke_profiles smoke_settings smoke_zoom

# Profile CRUD (no hardware needed)
./build/bin/smoke_settings

# Profile loading and switching
./build/bin/smoke_profiles

# Brightness control (requires running daemon + display)
./build/bin/smoke_brightness

# Zoom keystrokes (requires focused window)
./build/bin/smoke_zoom
```

## Troubleshooting

### Gadget shows "Wayland scroll not available"

The daemon is not running or is not reachable on D-Bus. Check:

```bash
systemctl --user status openwheel-daemon
gdbus introspect --session --dest org.asus.dial --object-path /org/asus/dial
```

### Dial not detected

```bash
# Check kernel recognises the device
dmesg | grep -i "asus\|hidraw" | tail -20

# List hidraw devices
ls -la /dev/hidraw*

# Verify udev rule applied
udevadm info --query=property --name=/dev/hidraw0 | grep -i "asus\|uaccess\|input"
```

### Brightness not changing on GNOME

OpenWheel uses `org.freedesktop.login1.Session.SetBrightness` (logind). Verify logind has a backlight device:

```bash
ls /sys/class/backlight/
```

If empty, your system may use ACPI brightness controls not exposed through sysfs. Try `brightnessctl list` and file a bug if the device is missing.

### Settings window doesn't open

The gadget must be running. Look for the tray icon. If the system tray is not available (some GNOME configurations), open settings via the profile picker: long-press the dial, then — currently — use `gdbus` to call `dialController.openSettings()` directly, or add the GNOME AppIndicator extension.
