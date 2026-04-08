# OpenWheel (GNOME fork)

A Linux driver and overlay UI for the **ASUS ProArt Dial** — the rotary dial built into ProArt Studiobook laptops and the ProArt Station PD2 display.

> **Fork notice:** This is a GNOME/Wayland-first fork of [fredaime/openwheel](https://github.com/fredaime/openwheel), which targets KDE Plasma. If you are on KDE, the upstream project may suit you better. This fork replaces KDE-specific input injection, window detection, and brightness/zoom backends with GNOME-native equivalents, and adds a built-in settings UI for remapping the dial without editing JSON.

Works natively on **GNOME/Wayland** (primary target) and KDE Plasma. No X11 required.

---

## How it works

```
ASUS Dial HID device (/dev/hidraw*)
  └─▶ asus_wheel daemon  (C)
        reads raw HID packets, broadcasts D-Bus signals
        exposes InjectKey / InjectScroll via /dev/uinput
  └─▶ openwheel-gadget  (C++20 / Qt 6 / QML)
        listens to D-Bus, executes actions, shows overlay HUD
```

All keystroke and scroll injection goes through `/dev/uinput` — the daemon creates a virtual input device at startup, so every keyboard shortcut works on Wayland without ydotool, xdotool, or any compositor-specific protocol.

---

## Features

- **Wayland-native input** — uinput virtual device; no X11 dependency for key injection
- **Press-to-activate mode** — actions only fire while the button is held (toggle in Settings)
- **Floating overlay HUD** — segmented arc gauge, function name, smooth animations; never steals keyboard focus from your app
- **Profile picker** — long-press the dial button, rotate to browse profiles, press to confirm
- **Per-application profiles** — JSON profiles for System, Music, Workspaces, Blender, Krita; enable/disable per profile in Settings
- **Settings window** — remap functions, add presets, create/delete profiles without editing JSON
- **Window switcher** — sticky Alt+Tab (Alt stays held across rotations, shows the GNOME task switcher)
- **GNOME brightness** — via `org.freedesktop.login1` (logind); falls back to `brightnessctl`
- **Volume** — via PipeWire/PulseAudio through `wpctl`
- **KDE Plasma compatible** — KDE Frameworks 6 is fully optional

---

## Requirements

### Runtime

| Dependency | Notes |
|---|---|
| Linux kernel 5.4+ | `/dev/uinput` support |
| D-Bus (session bus) | Signal transport between daemon and gadget |
| PipeWire or PulseAudio | Volume control via `wpctl` |
| `brightnessctl` *(optional)* | Brightness fallback if logind doesn't expose backlight |
| `playerctl` *(optional)* | Song seek in the Music profile (`sudo dnf install playerctl` / `sudo apt install playerctl`) |

### Build

| Dependency | Fedora package | Ubuntu/Debian package |
|---|---|---|
| CMake 3.16+ | `cmake` | `cmake` |
| C11 + C++20 compiler | `gcc-c++` | `build-essential` |
| libdbus-1 | `dbus-devel` | `libdbus-1-dev` |
| Qt 6 (Core, Gui, Qml, Quick, QuickControls2, DBus) | `qt6-qtbase-devel qt6-qtdeclarative-devel` | `qt6-base-dev qt6-declarative-dev` |
| X11 + XTest *(optional)* | `libX11-devel libXtst-devel` | `libx11-dev libxtst-dev` |
| KDE Frameworks 6 *(optional)* | `kf6-kcoreaddons-devel kf6-ki18n-devel kf6-kdbusaddons-devel` | `libkf6coreaddons-dev libkf6i18n-dev libkf6dbusaddons-dev` |

---

## Quick start — Fedora GNOME

```bash
# 1. Install build dependencies
sudo dnf install cmake ninja-build gcc-c++ dbus-devel \
    qt6-qtbase-devel qt6-qtdeclarative-devel \
    libX11-devel libXtst-devel

# 2. Clone and build
git clone https://github.com/jupist/openwheel-gnome.git
cd openwheel
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 3. Install system-wide
sudo cmake --install build --prefix /usr
```

`cmake --install` puts everything in the right places:

| What | Where |
|---|---|
| `asus_wheel` daemon | `/usr/bin/asus_wheel` |
| `openwheel-gadget` | `/usr/bin/openwheel-gadget` |
| udev rules | `/usr/lib/udev/rules.d/99-openwheel.rules` |
| systemd user service | `/usr/lib/systemd/user/openwheel-daemon.service` |
| Profiles | `/usr/share/openwheel/profiles/` |
| App icon | `/usr/share/icons/hicolor/scalable/apps/openwheel.svg` |
| Desktop / autostart | `/usr/share/applications/` and `/etc/xdg/autostart/` |

```bash
# 4. Apply udev rules (one-time per boot until next install)
sudo udevadm control --reload-rules && sudo udevadm trigger

# 5. Add yourself to the input group (takes effect after re-login)
sudo usermod -aG input $USER

# 6. Log out and back in, then enable the daemon and start the gadget
systemctl --user enable --now openwheel-daemon
# The gadget autostarts on login via /etc/xdg/autostart/
# To start it right now without logging out:
openwheel-gadget &
```

> **Why no root?** The udev rules grant device access to the `input` group. The daemon must run as your normal user so it can connect to your session D-Bus. Running `sudo asus_wheel` will fail because root is rejected by the user session bus socket.

---

## Quick start — Ubuntu/Debian GNOME

```bash
sudo apt-get install cmake ninja-build build-essential libdbus-1-dev \
    qt6-base-dev qt6-declarative-dev libx11-dev libxtst-dev

git clone https://github.com/jupist/openwheel-gnome.git
cd openwheel
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build --prefix /usr

sudo udevadm control --reload-rules && sudo udevadm trigger
sudo usermod -aG input $USER
# Log out and back in, then:
systemctl --user enable --now openwheel-daemon
openwheel-gadget &
```

---

## Using the dial

### Press-to-activate ON (default)

| Gesture | Effect |
|---|---|
| Hold button + rotate | Adjust the current function |
| Hold button + rotate fast | Accelerated adjustment (up to 4×) |
| Press and release (no rotation) | Execute the function's click action |
| Double-press | Cycle to the next function |
| Long-press (600 ms, no rotation) | Open profile picker |

### Press-to-activate OFF

| Gesture | Effect |
|---|---|
| Rotate | Adjust the current function |
| Press and release | Execute the function's click action |
| Double-press | Cycle to the next function |
| Long-press | Open profile picker |

### Profile picker

| Gesture | Effect |
|---|---|
| Rotate | Browse profiles (2 ticks per step) |
| Press | Confirm and switch to selected profile |
| Select "⚙ Settings" | Open the Settings window |

---

## Settings window

Open via the profile picker (scroll to the last entry "⚙ Settings" and press).

- **Behaviour** — toggle press-to-activate on/off
- **Profile list** — the dot on the right of each profile enables/disables it in the picker; disabled profiles are still editable
- **Function editor** — label, icon, type (continuous/discrete), unit, min/max value
- **Action editor** — keyboard shortcut, mouse scroll, D-Bus call, or shell command; includes a **Test** button to fire the action immediately
- **Presets** — Window Switcher (Alt+Tab sticky rotation)

Changes are saved to `~/.local/share/openwheel/profiles/` and survive package upgrades.

---

## Bundled profiles

| Profile | Enabled by default | Functions |
|---|---|---|
| **System** | Yes | Volume, Brightness, Zoom, Scroll, Window Switcher |
| **Music** | Yes | Skip Track (CW=next, CCW=prev; click=play/pause), Volume, Seek (CW=+10s, CCW=−10s via `playerctl`) |
| **Workspaces** | Yes | Switch Workspace (Super+PgDn/Up) |
| **Blender** | No | Timeline scrub, Undo/Redo, Zoom, Scroll |
| **Krita** | No | Brush Size (]/[ keys), Undo/Redo, Zoom, Scroll |

Enable Blender or Krita in Settings → click the dot next to the profile name.

---

## Profile JSON format

Profiles live in `/usr/share/openwheel/profiles/` (system) or `~/.local/share/openwheel/profiles/` (user — takes precedence).

```jsonc
{
  "id": "my-app",
  "displayName": "My App",
  "enabled": true,
  "processNames": ["myapp"],         // substring match on process name
  "windowClassPattern": "^MyApp$",   // regex on WM_CLASS
  "functions": [
    {
      "id": "brush-size",
      "label": "Brush Size",
      "iconName": "draw-brush",       // freedesktop icon name
      "type": "continuous",           // "continuous" | "discrete"
      "unit": "px",
      "minValue": 1,
      "maxValue": 500,
      "clockwiseAction": {
        "type": "keyboard",
        "keys": "bracketright",       // see key names below
        "modifiers": [],              // "ctrl" | "shift" | "alt" | "super"
        "rotationThreshold": 15,      // degrees per trigger
        "accelerationEnabled": true
      },
      "counterClockwiseAction": { "type": "keyboard", "keys": "bracketleft" },
      "clickAction":               { "type": "keyboard", "keys": "r" }
    }
  ],
  "defaultMenuLayout": ["brush-size"],
  "defaultFunctionId": "brush-size"
}
```

### Action types

| Type | Required fields | Description |
|---|---|---|
| `keyboard` | `keys` | Single keystroke per threshold crossing |
| `keyboardRepeat` | `keys` | Repeated keystrokes proportional to rotation |
| `mouseScroll` | `delta` | Scroll wheel simulation |
| `dbus` | `dbusService`, `dbusPath`, `dbusInterface`, `dbusMethod`, `dbusArgs` | D-Bus method call |
| `command` | `command` | Shell command (run via `/bin/sh -c`) |

### Special key names

In addition to standard names (`a`–`z`, `f1`–`f12`, `left`, `right`, `space`, `return`, `escape`, …):

| Name | Effect |
|---|---|
| `XF86AudioRaiseVolume` / `XF86AudioLowerVolume` | Volume via wpctl |
| `XF86AudioMute` | Toggle mute |
| `XF86AudioPlay` / `XF86AudioNext` / `XF86AudioPrev` | Media playback control |
| `XF86MonBrightnessUp` / `XF86MonBrightnessDown` | Screen brightness |
| `ZoomIn` / `ZoomOut` | App zoom (Ctrl+= / Ctrl+−) |
| `super` / `super_l` / `super_r` | Super / Meta key |

---

## Architecture

```
openwheel-daemon/
  hidreader.c        HID reader, D-Bus signal broadcaster, startup/teardown
  uinput.c           /dev/uinput virtual keyboard + pointer device
  dbus_methods.c     InjectKey / InjectScroll / HoldModifiers / ReleaseModifiers
  keymap.c           Key name → Linux input event code lookup table

openwheel-gadget/src/
  core/
    dialcontroller.cpp      Orchestrates all components; Q_PROPERTY / Q_INVOKABLE for QML
    actionexecutor.cpp      Dispatch chain: uinput → logind/wpctl → X11 XTest fallback
    rotationhandler.cpp     Velocity-based acceleration (exponential smoothing, max 4×)
    desktopenvironment.cpp  Runtime DE / session detection (XDG_CURRENT_DESKTOP)
  profiles/
    profilemanager.cpp      Load / save / create / delete (XDG data dirs, user overrides)
    profile.cpp             JSON (de)serialisation for Profile, Function, ActionConfig
    applicationmatcher.cpp  Manual profile selection shim
  ui/
    OverlayWindow.qml            Floating HUD + profile picker
    settings/
      SettingsWindow.qml         Profile list + behaviour toggles
      FunctionEditor.qml         Per-function editor
      ActionEditor.qml           Per-action editor with Test button
      WindowSwitcherPreset.qml   Alt+Tab sticky preset

profiles/    Bundled JSON profiles
udev/        99-openwheel.rules
systemd/     openwheel-daemon.service.in
```

---

## Troubleshooting

### Daemon exits immediately / "D-Bus connect failed"

Do not start with `sudo`. Use the udev rules instead:

```bash
# Check device permissions (should show rw for all):
ls -la /dev/hidraw* /dev/uinput

# Quick fix for this session (survives until reboot):
sudo chmod 666 /dev/hidraw1 /dev/uinput

# Permanent fix: install udev rules + join input group (see Quick Start)
```

### Keys or scroll not working on Wayland

Verify the daemon's uinput injector is available:

```bash
gdbus call --session \
  --dest org.asus.dial \
  --object-path /org/asus/dial \
  --method org.asus.dial.InjectionAvailable
# Expected output: (true,)
```

If it returns `(false,)`, `/dev/uinput` is not accessible to the daemon.

### Overlay appears but rotating does nothing

Press-to-activate is on by default — hold the dial button while rotating.
Toggle it off in Settings → Behaviour if you prefer free rotation.

### Settings window doesn't open

In the profile picker, scroll to the last entry ("⚙ Settings") and press the button.

---

## Build options

```bash
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_DAEMON=ON \
  -DBUILD_GADGET=ON \
  -DBUILD_TESTS=ON
cmake --build build
cd build && ctest --output-on-failure
```

| Option | Default | Description |
|---|---|---|
| `BUILD_DAEMON` | `ON` | Build `asus_wheel` |
| `BUILD_GADGET` | `ON` | Build `openwheel-gadget` |
| `BUILD_TESTS` | `ON` | Build and register unit tests |
| `ENABLE_INSTALL` | `ON` | Generate `cmake --install` targets |

---

## Tested on

- ASUS ProArt Studiobook 16 (`ASUS2020:00 0B05:0220`)
- GNOME 46+ on Wayland (Fedora 40+)
- KDE Plasma 6 on Wayland
- PipeWire audio

---

## Contributing

Pull requests are welcome. Please keep the daemon in plain C11 (no C++) and the gadget in C++20 / Qt 6 QML. Run `ctest` before submitting.

---

## License

GPL-3.0-or-later — see [LICENSE](LICENSE).
