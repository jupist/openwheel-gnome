# OpenWheel Creative Dial Controller - Implementation Summary

## Overview

This implementation provides a comprehensive creative dial controller for KDE Plasma 6, based on the detailed software specification. The system connects to the existing `openwheel-daemon` and provides rich functionality for controlling creative applications with a rotary dial input device.

## Implementation Status

### ✅ Completed Components

#### 1. Core C++ Classes

- **DBusInterface** (`src/dbus/dbusinterface.h/cpp`)
  - Connects to the openwheel-daemon via D-Bus
  - Listens for rotation and button press signals from `org.asus.dial`
  - Translates hardware events into application signals

- **Profile System** (`src/profiles/profile.h/cpp`)
  - Complete data structures for profiles, functions, and actions
  - JSON serialization/deserialization
  - Support for multiple action types: keyboard, mouse scroll, D-Bus calls, commands
  - Regex-based window matching
  - Continuous and discrete function types

- **ProfileManager** (`src/profiles/profilemanager.h/cpp`)
  - Loads profiles from JSON files
  - Searches standard XDG data directories
  - Automatic profile switching based on active window
  - Fallback to default system profile

- **ApplicationMatcher** (`src/profiles/applicationmatcher.h/cpp`)
  - Detects active window using KWindowSystem
  - Extracts process name, window class, and window title
  - Works on both X11 and Wayland (with KDE Frameworks)
  - Conditional compilation for systems without KDE Frameworks

- **ActionExecutor** (`src/core/actionexecutor.h/cpp`)
  - Simulates keyboard events using X11 XTest extension
  - Simulates mouse scroll events
  - Executes D-Bus method calls
  - Runs shell commands
  - Comprehensive key mapping for common shortcuts

- **RotationHandler** (`src/core/rotationhandler.h/cpp`)
  - Converts rotation deltas to actionable events
  - Implements acceleration curve for fast rotations
  - Configurable thresholds per function
  - Smooth velocity tracking
  - Adjusting state detection

- **DialController** (`src/core/dialcontroller.h/cpp`)
  - Main orchestrator connecting all components
  - Exposed to QML for UI integration
  - Manages active state and function selection
  - Coordinates profile switching
  - Handles rotation and button events

#### 2. JSON Profiles

Created three default profiles:

- **system-default.json**: Volume, brightness, scroll, zoom
- **krita.json**: Brush size/opacity, canvas rotation, zoom, undo/redo
- **blender.json**: Zoom, timeline navigation, undo/redo

Each profile includes:
- Function definitions with icons and labels
- Action configurations (clockwise/counterclockwise)
- Rotation thresholds and acceleration settings
- Application matching patterns

#### 3. Build System

- **CMakeLists.txt**: Comprehensive build configuration
  - Qt6/Qt5 fallback support
  - Optional KDE Frameworks 6 integration
  - X11/XTest detection
  - Conditional compilation flags
  - Profile installation rules

## Architecture

```
Hardware (ASUS Dial)
    ↓ HID events
openwheel-daemon (C)
    ↓ D-Bus signals (org.asus.dial)
DBusInterface
    ↓
DialController
    ├── ProfileManager ← loads profiles
    ├── ApplicationMatcher ← detects active window
    ├── RotationHandler ← processes rotation
    └── ActionExecutor ← simulates input
        ↓
Operating System / Applications
```

## Key Features

### Profile System
- **Automatic Switching**: Changes profile when you switch applications
- **Flexible Matching**: Match by process name, window class, or window title (regex)
- **Fallback**: Always has a default profile for unmatched applications
- **Extensible**: Easy to add new profiles via JSON files

### Action Types
1. **Keyboard**: Single key press (e.g., Ctrl+Z for undo)
2. **KeyboardRepeat**: Repeated keystrokes for continuous adjustment
3. **MouseScroll**: Scroll wheel simulation
4. **DBusCall**: Call D-Bus methods (for KDE integration)
5. **Command**: Execute shell commands

### Rotation Processing
- **Velocity Tracking**: Detects slow vs. fast rotation
- **Acceleration Curve**: Faster rotation = bigger steps
- **Configurable Thresholds**: Each function can have different sensitivity
- **Smooth Operation**: Remainder tracking for precise control

### Conditional Compilation
The code gracefully degrades when KDE Frameworks are not available:
- `NO_KDE_FRAMEWORKS`: Disables KDE-specific features
- `HAVE_X11`: Enables X11 keyboard/mouse simulation
- Still functional in minimal environments

## File Structure

```
openwheel/
├── openwheel-daemon/         # Existing HID reader (unchanged)
├── openwheel-gadget/         # New implementation
│   ├── src/
│   │   ├── main.cpp          # Application entry point
│   │   ├── core/             # Core controller logic
│   │   │   ├── dialcontroller.{h,cpp}
│   │   │   ├── rotationhandler.{h,cpp}
│   │   │   └── actionexecutor.{h,cpp}
│   │   ├── profiles/         # Profile management
│   │   │   ├── profile.{h,cpp}
│   │   │   ├── profilemanager.{h,cpp}
│   │   │   └── applicationmatcher.{h,cpp}
│   │   └── dbus/             # D-Bus communication
│   │       └── dbusinterface.{h,cpp}
│   ├── package/              # Plasma applet (existing, for future overlay)
│   └── CMakeLists.txt        # Build configuration
├── profiles/                 # JSON profile definitions
│   ├── system-default.json
│   ├── krita.json
│   └── blender.json
└── IMPLEMENTATION.md         # This file
```

## Dependencies

### Required
- Qt 6.5+ (Core, Gui, Widgets, Qml, Quick, DBus)
- CMake 3.16+
- C++20 compiler

### Optional (for full functionality)
- KDE Frameworks 6:
  - KF6::Config (settings)
  - KF6::CoreAddons (utilities)
  - KF6::I18n (localization)
  - KF6::WindowSystem (window detection)
  - KF6::DBusAddons (D-Bus helpers)
- X11 with XTest extension (for keyboard/mouse simulation)

## Building

```bash
cd openwheel/openwheel-gadget
mkdir build && cd build
cmake ..
make
sudo make install
```

## Usage

1. **Start the daemon**:
   ```bash
   openwheel-daemon  # or asus_wheel
   ```

2. **Start the gadget**:
   ```bash
   openwheel-gadget
   ```

3. **Use the dial**:
   - Rotate to trigger actions in the current application
   - Press the button to show/hide the overlay (future feature)
   - The system automatically switches profiles when you change windows

## Configuration

### Adding New Profiles

Create a JSON file in one of these locations:
- `/usr/share/openwheel/profiles/`
- `~/.local/share/openwheel/profiles/`
- `./profiles/` (development)

Example structure:
```json
{
  "id": "my-app",
  "displayName": "My Application",
  "processNames": ["myapp", "myapp.exe"],
  "windowClassPattern": "^MyApp$",
  "functions": [
    {
      "id": "zoom",
      "label": "Zoom",
      "iconName": "zoom-in",
      "type": "continuous",
      "clockwiseAction": {
        "type": "keyboard",
        "keys": "plus",
        "modifiers": ["ctrl"],
        "rotationThreshold": 20
      },
      "counterClockwiseAction": {
        "type": "keyboard",
        "keys": "minus",
        "modifiers": ["ctrl"],
        "rotationThreshold": 20
      }
    }
  ],
  "defaultMenuLayout": ["zoom"]
}
```

## Future Work

### Not Yet Implemented (from original spec)

1. **QML Overlay UI**
   - RadialMenu.qml for visual function selection
   - MenuSegment.qml for individual segments
   - ValueIndicator.qml for current value display
   - OverlayWindow for always-on-top display

2. **Settings Management**
   - KConfig integration for user preferences
   - Settings dialog (KCModule)
   - Shortcut customization
   - Acceleration curve tuning

3. **System Integration**
   - KGlobalAccel for global shortcuts
   - KStatusNotifierItem for system tray
   - Autostart desktop file
   - D-Bus service definition

4. **Advanced Features**
   - Per-function value tracking
   - History/undo for dial operations
   - Profile editor GUI
   - Haptic feedback (if hardware supports)

## Technical Decisions

### Why D-Bus?
The existing `openwheel-daemon` uses D-Bus for IPC, making it the natural choice for communication. This allows the daemon and gadget to run as separate processes.

### Why Separate Profile Manager?
Profiles can be large and complex. A dedicated manager:
- Caches loaded profiles
- Handles file system watching (future)
- Supports multiple search paths
- Provides clean API for switching

### Why Acceleration Curve?
Users rotate dials at different speeds:
- **Slow rotation**: Fine control (1° per action)
- **Fast rotation**: Coarse control (4° per action)
- Makes the dial feel responsive and natural

### Why Conditional KDE Support?
- Allows building/testing in minimal environments
- Makes the code portable
- KDE features are optional enhancements, not core requirements
- Degrades gracefully

## Code Quality

- **Modern C++20**: Uses std::unique_ptr, std::optional, range-based loops
- **Qt Best Practices**: Signal/slot connections, Q_PROPERTY for QML
- **Memory Safety**: RAII, smart pointers, no raw new/delete
- **Error Handling**: Defensive coding, null checks, fallback values
- **Documentation**: Comprehensive comments, doxygen-ready
- **Logging**: qDebug() for diagnostics and troubleshooting

## Testing

To test the implementation:

1. **Daemon Test**: Run `openwheel-daemon` and check D-Bus signals:
   ```bash
   dbus-monitor --session "type='signal',interface='org.asus.dial'"
   ```

2. **Profile Loading**: The gadget logs which profiles it loads on startup

3. **Window Matching**: Switch between applications and check debug output

4. **Action Execution**: Rotate the dial and verify keyboard events are simulated

## Contributing

When adding new features:
1. Follow the existing code style
2. Add appropriate conditional compilation for optional features
3. Update this documentation
4. Test with and without KDE Frameworks
5. Consider X11 vs. Wayland compatibility

## License

GPL-3.0-or-later

## Authors

- OpenWheel Contributors
- Based on the comprehensive KDE Plasma Creative Dial specification

---

*Last Updated: 2026-01-10*
*Implementation Version: 0.1.0*
