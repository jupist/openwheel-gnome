# OpenWheel (GNOME fork)

OpenWheel turns the ASUS ProArt Dial (and compatible ASUS HID rotary devices) into a fully-customisable system control dial on Linux. Works natively on **GNOME/Wayland** and KDE Plasma.

Rotate to adjust volume, brightness, or run any keyboard shortcut. A floating overlay shows the current function and value. Long-press the dial button to pick a profile; double-click to cycle functions within a profile. Open the **Settings** window from the tray icon to remap everything without editing JSON.

## How it works

```
ASUS Dial HID (/dev/hidraw*)
  → asus_wheel daemon  (C, reads raw HID packets)
  → D-Bus signals      (org.asus.dial: Rotate, Press)
  → openwheel-gadget   (C++20/Qt 6/QML, executes actions + shows overlay)
```

The daemon also exposes `InjectKey` / `InjectScroll` D-Bus methods backed by a `/dev/uinput` virtual device, so all keystroke and scroll actions work natively on Wayland — no X11 required.

## Features

- **Wayland-native input** — keystrokes and scroll events via `/dev/uinput`; no ydotool or xdotool dependency
- **GNOME brightness** — adjusts screen brightness through `org.freedesktop.login1` (logind), works on any composited session
- **Per-application profiles** — JSON profiles for Blender, Krita, and the system default; switch profiles with a long press or from the tray menu
- **Qt/QML settings UI** — remap functions, add presets (e.g. window-switcher), create and delete profiles without editing JSON
- **Floating overlay** — segmented gauge ring, smooth animations, profile picker on long press
- **KDE Plasma compatible** — KDE Frameworks 6 is fully optional; when present it enables single-instance enforcement

---

| Dependency | Required | Notes |
|---|---|---|
| CMake 3.16+ | ✓ | |
| C11 compiler | ✓ | For daemon |
| C++20 compiler | ✓ | For gadget |
| libdbus-1 | ✓ | |
| Qt 6 (Core, Gui, Widgets, Qml, Quick, QuickControls2, DBus) | ✓ | Qt 5.15 also works |
| X11 + XTest | optional | Fallback key injection under X11 sessions |
| KDE Frameworks 6 | optional | Adds KDBusService single-instance guard |

## Quick Start (Fedora GNOME)

```bash
# 1. Install build dependencies
sudo dnf install cmake ninja-build gcc-c++ pkg-config dbus-devel \
    qt6-qtbase-devel qt6-qtdeclarative-devel \
    libX11-devel libXtst-devel

# 2. Clone and build
git clone https://github.com/fredaime/openwheel.git
cd openwheel
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 3. Run tests
cd build && ctest --output-on-failure && cd ..

# 4. Install (to /usr so udev and systemd paths are correct)
sudo cmake --install build --prefix /usr

# 5. Set up device access (one-time)
sudo udevadm control --reload
sudo udevadm trigger
sudo usermod -aG input $USER   # log out and back in after this

# 6. Enable autostart
systemctl --user enable --now openwheel-daemon
# The gadget autostarts on login via /etc/xdg/autostart/openwheel-gadget.desktop
# To start it right now without logging out:
openwheel-gadget &
```

## Quick Start (Ubuntu/Debian GNOME)

```bash
sudo apt-get install cmake ninja-build build-essential pkg-config libdbus-1-dev \
    qt6-base-dev qt6-declarative-dev libx11-dev libxtst-dev

git clone https://github.com/fredaime/openwheel.git
cd openwheel
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build --prefix /usr
sudo udevadm control --reload && sudo udevadm trigger
sudo usermod -aG input $USER
# Log out and back in, then:
systemctl --user enable --now openwheel-daemon
openwheel-gadget &
```

## Using the dial

| Action | Effect |
|---|---|
| Rotate | Adjust the current function (volume, brightness, …) |
| Double-click button | Cycle to the next function in the profile |
| Long-press button (600 ms) | Open the profile picker overlay |
| Rotate while picker is open | Browse profiles |
| Click/short-press while picker is open | Confirm profile switch |
| Tray icon left-click | Open Settings window |
| Tray icon right-click → Settings | Open Settings window |

## Settings window

Click the tray icon (or right-click → Settings) to open the profile editor:

- **Profile list** — create, rename, delete profiles
- **Function list** — add or remove functions per profile
- **Action editor** — map clockwise/counter-clockwise/click to keyboard shortcuts, scroll, D-Bus calls, or shell commands
- **Test button** — fire an action immediately to verify it works
- **Window-switcher preset** — one click adds an Alt+Tab / Alt+Shift+Tab / Super function

Edited profiles are saved to `~/.local/share/openwheel/profiles/` and survive package upgrades.

## Using the dial

Bundled profiles (in `/usr/share/openwheel/profiles/` after install):

| Profile | Functions |
|---|---|
| **System** (default) | Volume, Brightness, Zoom, Scroll |
| **Blender** | Viewport zoom, Timeline scrub, Undo/Redo |
| **Krita** | Brush size, Brush opacity, Canvas rotation, Zoom |

User-created profiles live in `~/.local/share/openwheel/profiles/` and take precedence over system profiles with the same ID.

### Profile JSON format

### Function picker (double-press)

| Gesture | Effect |
|---|---|
| Rotate | Browse functions in the current profile |
| Press | Confirm and switch to selected function |

### Profile picker (long-press)

| Gesture | Effect |
|---|---|
| Rotate | Browse profiles (2 ticks per step) |
| Press | Confirm and switch to selected profile |
| Select "⚙ Settings" | Open the Settings window |

---

## Settings window

Open via the profile picker (long-press the dial, scroll to "⚙ Settings", press to confirm).

- **`?` help button** — step-by-step guide to creating, editing, and deleting profiles
- **Behaviour** — toggle press-to-activate on/off; toggle daemon autostart on login
- **Profile list** — the dot on the right of each profile enables/disables it in the picker; disabled profiles are still editable
- **Function editor** — label, type (continuous/discrete), unit; expand a card to edit
- **Action editor** — keyboard shortcut, mouse scroll, D-Bus call, or shell command; **Test** button fires the action immediately
- **Unsaved changes** — the title bar shows `*` when changes are pending; closing without saving discards all edits (Ctrl+S to save)

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
  "functions": [
    {
      "id": "brush-size",
      "label": "Brush Size",
      "iconName": "draw-brush",
      "type": "continuous",
      "unit": "px",
      "clockwiseAction":        { "type": "keyboard", "keys": "bracketright" },
      "counterClockwiseAction": { "type": "keyboard", "keys": "bracketleft"  },
      "clickAction":            { "type": "keyboard", "keys": "Return"       }
    }
  ],
  "defaultMenuLayout": ["brush-size"]
}
```

Action types: `keyboard`, `keyboardRepeat`, `mouseScroll`, `dbus`, `command`.

## Architecture

```
openwheel-daemon/
  hidreader.c      — HID packet reader, D-Bus signal broadcaster
  uinput.c         — /dev/uinput virtual device (keystroke + scroll injection)
  dbus_methods.c   — InjectKey / InjectScroll / ListSupportedKeys D-Bus methods
  keymap.c         — name → Linux keycode table

openwheel-gadget/src/
  core/
    dialcontroller.cpp    — orchestrates everything; exposed to QML
    actionexecutor.cpp    — dispatches to uinput daemon / logind / X11 fallback
    desktopenvironment.cpp — runtime DE/session detection
    rotationhandler.cpp   — velocity-based acceleration
  profiles/
    profilemanager.cpp    — load/save/create/delete profiles
    profile.cpp           — JSON (de)serialisation
    applicationmatcher.cpp — manual profile selection shim (GNOME mode)
  ui/
    OverlayWindow.qml     — floating HUD + profile picker
    settings/
      SettingsWindow.qml  — profile + function editor
      FunctionEditor.qml  — per-function inline editor
      ActionEditor.qml    — per-action-slot editor with Test button
      WindowSwitcherPreset.qml — Alt+Tab preset button
```

| Type | Required fields | Description |
|---|---|---|
| `keyboard` | `keys` | Single keystroke per threshold crossing |
| `keyboardRepeat` | `keys` | Repeated keystrokes proportional to rotation |
| `mouseScroll` | `delta` | Scroll wheel simulation |
| `dbus` | `dbusService`, `dbusPath`, `dbusInterface`, `dbusMethod`, `dbusArgs` | D-Bus method call |
| `command` | `command` | Shell command (run via `/bin/sh -c`) |

Tested with:
- ASUS ProArt Dial (I2C HID, `ASUS2020`)
- GNOME 46+ on Wayland (Fedora 40+)
- KDE Plasma 6 on Wayland
- PipeWire audio

## License

MIT — see [LICENSE](LICENSE).
