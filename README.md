# OpenWheel

OpenWheel turns the ASUS Dial (and similar I2C/USB HID rotary devices) into a system-wide control dial on Linux. Rotate to adjust volume, brightness, or compositor zoom; double-click the button to switch functions. A floating overlay widget shows the current value in real time.

## How It Works

```
ASUS Dial HID (/dev/hidraw*)
  → openwheel-daemon  (C, reads HID packets)
  → D-Bus signals     (org.asus.dial: Rotate, Press)
  → openwheel-gadget  (C++20/Qt 6, executes actions + shows overlay)
```

**openwheel-daemon** reads raw HID events and broadcasts rotation/button signals over the session D-Bus.

**openwheel-gadget** listens on D-Bus, maps rotation to per-application actions, and displays a 3D dial overlay widget at the bottom of the screen.

## Features

- **Wayland-native** — volume via PipeWire (`wpctl`), brightness via KDE PowerDevil, fullscreen zoom via KWin compositor (no X11 required)
- **Per-application profiles** — auto-switches dial functions based on the active window (Blender, Krita, or custom JSON profiles)
- **3D overlay widget** — two concentric circles with segmented gauge ring, Kirigami theme integration, spring/pulse animations
- **Double-click to cycle** — button double-click switches between Volume, Brightness, Zoom, and Scroll
- **X11 fallback** — XTest input simulation when running under X11

## Prerequisites

- CMake 3.16+
- Qt 6 (Core, Gui, Widgets, Qml, Quick, QuickControls2, DBus)
- KDE Frameworks 6 (optional — enables window tracking for auto profile switching)
- D-Bus (`libdbus-1`)
- X11 + XTest (optional — for X11 input simulation fallback)

## Build & Install

```bash
git clone https://github.com/fredaime/openwheel.git
cd openwheel

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Run tests
cd build && ctest --output-on-failure && cd ..

# Install (default prefix: /usr/local)
sudo cmake --install build
```

Build options: `-DBUILD_DAEMON=ON`, `-DBUILD_GADGET=ON`, `-DBUILD_TESTS=ON`

## Running

The daemon needs read access to the HID device. Either run as root, or set up a udev rule:

```bash
# Option 1: temporary permission
sudo chmod a+r /dev/hidraw*

# Option 2: udev rule (persistent)
echo 'SUBSYSTEM=="hidraw", ATTRS{idVendor}=="0b05", MODE="0644"' | \
  sudo tee /etc/udev/rules.d/99-openwheel.rules
sudo udevadm control --reload-rules
```

Start both processes:

```bash
# Start the daemon (reads HID events, broadcasts D-Bus signals)
asus_wheel &

# Start the gadget (listens for signals, shows overlay)
openwheel-gadget
```

Rotate the dial to adjust the current function. Double-click the button to cycle through functions (Volume → Brightness → Zoom → Scroll).

## Profiles

JSON profiles in `$XDG_DATA_DIRS/openwheel/profiles/` define per-application dial functions. Included profiles:

| Profile | Functions |
|---------|-----------|
| **System** (default) | Volume, Brightness, Zoom, Scroll |
| **Blender** | Viewport zoom, Timeline scrub, Undo/Redo |
| **Krita** | Brush size, Brush opacity, Canvas rotation, Zoom |

Profiles match the active window by process name, window class regex, or window title regex.

### Profile format

```json
{
  "id": "my-app",
  "displayName": "My App",
  "processNames": ["myapp"],
  "functions": [
    {
      "id": "volume",
      "label": "Volume",
      "iconName": "audio-volume-high",
      "type": "continuous",
      "minValue": 0, "maxValue": 100, "unit": "%",
      "clockwiseAction": { "type": "keyboard", "keys": "XF86AudioRaiseVolume", "rotationThreshold": 5 },
      "counterClockwiseAction": { "type": "keyboard", "keys": "XF86AudioLowerVolume", "rotationThreshold": 5 }
    }
  ],
  "defaultMenuLayout": ["volume"]
}
```

Action types: `keyboard`, `mouseScroll`, `dbus`, `command`.

## Compatibility

Tested with:
- ASUS Dial (I2C HID, `ASUS2020` device identifier)
- KDE Plasma 6 on Wayland
- PipeWire audio

The daemon detects devices by scanning `/sys/class/hidraw/` for `ASUS2020` in either `device/product` (USB) or `device/uevent` (I2C).

## License

GPL-3.0-or-later. See [LICENSE](LICENSE).
