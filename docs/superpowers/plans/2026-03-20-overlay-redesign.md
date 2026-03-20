# Overlay Redesign & Wayland-Native Actions

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace flat overlay with a 3D two-circle dial widget, add double-click to switch functions, wire Zoom to KWin compositor zoom, and display real system values.

**Architecture:** DialController gains double-click detection (button timestamp tracking). ActionExecutor gains KWin zoom via `org.kde.kglobalaccel` D-Bus shortcut invocation. OverlayWindow.qml is rewritten as two concentric circles: a back gauge ring (120% size, segmented arc) and a front info circle (drop shadow, icon/text). Colors from Kirigami.Theme. Show/hide uses spring scale+fade animations.

**Tech Stack:** Qt 6 / QML, Kirigami theme, D-Bus (wpctl, PowerDevil, KGlobalAccel), Canvas 2D for gauge ring.

**Prefer Qt types over STL throughout.**

---

### Task 1: Double-Click Detection in DialController

**Files:**
- Modify: `openwheel-gadget/src/core/dialcontroller.h`
- Modify: `openwheel-gadget/src/core/dialcontroller.cpp`

- [ ] **Step 1: Add double-click state to header**

In `dialcontroller.h`, add to private members:

```cpp
#include <QElapsedTimer>

// After existing members:
QElapsedTimer m_buttonClickTimer;
int m_clickCount = 0;
QTimer *m_clickDecisionTimer = nullptr;
static constexpr int DOUBLE_CLICK_INTERVAL = 400; // ms
```

Add signal:

```cpp
void functionCycled(); // emitted on double-click
```

- [ ] **Step 2: Implement double-click logic in .cpp**

Replace `onButtonPressed` and `onButtonReleased` in `dialcontroller.cpp`:

```cpp
void DialController::onButtonPressed()
{
    qDebug() << "DialController: Button pressed";
    // Just track press, action happens on release
}

void DialController::onButtonReleased()
{
    qDebug() << "DialController: Button released";

    m_clickCount++;

    if (m_clickCount == 1) {
        // Start decision timer - wait to see if second click comes
        if (!m_clickDecisionTimer) {
            m_clickDecisionTimer = new QTimer(this);
            m_clickDecisionTimer->setSingleShot(true);
            m_clickDecisionTimer->setInterval(DOUBLE_CLICK_INTERVAL);
            connect(m_clickDecisionTimer, &QTimer::timeout, this, [this]() {
                if (m_clickCount == 1) {
                    // Single click - no action (reserved for future)
                    qDebug() << "Single click (ignored)";
                }
                m_clickCount = 0;
            });
        }
        m_clickDecisionTimer->start();
    } else if (m_clickCount >= 2) {
        // Double click - cycle function
        m_clickDecisionTimer->stop();
        m_clickCount = 0;
        selectNext();
        Q_EMIT functionCycled();
        qDebug() << "Double click -> next function";
    }
}
```

Remove the `activate()`/`deactivate()` calls from button handlers — double-click now cycles functions instead.

- [ ] **Step 3: Build and verify**

Run: `cmake --build build --target openwheel-gadget 2>&1`
Expected: compiles cleanly

- [ ] **Step 4: Commit**

```bash
git add openwheel-gadget/src/core/dialcontroller.h openwheel-gadget/src/core/dialcontroller.cpp
git commit -m "feat: double-click button cycles dial function"
```

---

### Task 2: KWin Compositor Zoom Action

**Files:**
- Modify: `openwheel-gadget/src/core/actionexecutor.h`
- Modify: `openwheel-gadget/src/core/actionexecutor.cpp`
- Modify: `profiles/system-default.json`

- [ ] **Step 1: Add zoomChange method**

In `actionexecutor.h`, add to private:

```cpp
void zoomChange(int direction);
```

- [ ] **Step 2: Implement KWin zoom via D-Bus**

In `actionexecutor.cpp`, add the method and register it in `tryWaylandKeyAction`:

```cpp
void ActionExecutor::zoomChange(int direction)
{
    auto bus = QDBusConnection::sessionBus();
    QString shortcut = (direction > 0) ? QStringLiteral("view_zoom_in")
                                       : QStringLiteral("view_zoom_out");

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QStringLiteral("org.kde.kglobalaccel"),
        QStringLiteral("/component/kwin"),
        QStringLiteral("org.kde.kglobalaccel.Component"),
        QStringLiteral("invokeShortcut"));
    msg << shortcut;
    bus.call(msg, QDBus::NoBlock);

    qDebug() << "Zoom:" << shortcut;
}
```

Add to `tryWaylandKeyAction`:

```cpp
if (keys == QStringLiteral("ZoomIn")) {
    zoomChange(+1);
    return true;
}
if (keys == QStringLiteral("ZoomOut")) {
    zoomChange(-1);
    return true;
}
```

- [ ] **Step 3: Update system-default.json zoom function**

Replace the zoom section in `profiles/system-default.json`:

```json
{
    "id": "zoom",
    "label": "Zoom",
    "iconName": "zoom-in",
    "type": "continuous",
    "clockwiseAction": {
        "type": "keyboard",
        "keys": "ZoomIn",
        "rotationThreshold": 5
    },
    "counterClockwiseAction": {
        "type": "keyboard",
        "keys": "ZoomOut",
        "rotationThreshold": 5
    }
}
```

Note: Remove minValue/maxValue/unit from zoom — compositor zoom has no bounded range.

- [ ] **Step 4: Build and verify**

Run: `cmake --build build --target openwheel-gadget 2>&1`
Expected: compiles cleanly

- [ ] **Step 5: Commit**

```bash
git add openwheel-gadget/src/core/actionexecutor.h openwheel-gadget/src/core/actionexecutor.cpp profiles/system-default.json
git commit -m "feat: zoom uses KWin compositor zoom via D-Bus"
```

---

### Task 3: Rewrite Overlay QML — Two-Circle 3D Design

**Files:**
- Rewrite: `openwheel-gadget/src/ui/OverlayWindow.qml`

This is the main visual overhaul. Two concentric circles with depth.

- [ ] **Step 1: Rewrite OverlayWindow.qml**

Complete replacement of the QML file. Key structure:

```
Window (transparent, frameless, stays-on-top, click-through)
├── content (Item, animations target)
│   ├── backCircle (120% size, offset Y+4 for depth)
│   │   └── gaugeCanvas (Canvas: segmented arc gauge for value range)
│   ├── frontCircle (100% size)
│   │   ├── dropShadow (Rectangle, offset Y+6, blur via gradient)
│   │   ├── circleBody (Rectangle, Kirigami.Theme.backgroundColor)
│   │   ├── ColumnLayout (icon, function name, value, profile)
│   │   └── functionDots (Row of dots at bottom, visible on function cycle)
│   └── feedbackRing (pulse animation)
```

Design details:

- **Window**: 320×320, centered at bottom of screen, `Qt.WindowTransparentForInput`
- **Back circle** (384×384, centered): Semi-transparent, contains a `Canvas` that draws segmented arc sections. Filled segments up to normalized value, unfilled segments in dim color. 20 segments. Colors from `Kirigami.Theme.highlightColor` for filled, `Kirigami.Theme.backgroundColor` darkened for unfilled.
- **Front circle** (280×280, centered): Solid `Kirigami.Theme.backgroundColor` with 1px `Kirigami.Theme.separatorColor` border. Drop shadow: a duplicate circle offset Y+6 with `#40000000` color, slightly larger.
- **Icon**: Use `Kirigami.Icon` with `source` from function iconName (freedesktop icon names: `audio-volume-high`, `display-brightness`, `zoom-in`, `view-scroll-vertical`).
- **Text colors**: `Kirigami.Theme.textColor` for labels, `Kirigami.Theme.highlightColor` for value.
- **Show animation**: `ParallelAnimation` { scale 0.85→1.0 (spring), opacity 0→1 (150ms) }
- **Hide animation**: `ParallelAnimation` { scale 1.0→0.9 (100ms), opacity 1→0 (200ms) }
- **Rotation tick**: quick scale pulse 1.0→1.03→1.0 (200ms) on the front circle
- **Function cycle**: front circle briefly scales to 0.95 then back to 1.0 (bounce)
- **Value arc transition**: `Behavior on normalizedValue { NumberAnimation { duration: 120 } }` for smooth gauge movement

Connections to controller:
- `dialController.valueChanged(value)` → update `currentValue`, repaint gauge
- `dialController.rotationTick()` → show overlay if hidden, restart hide timer, pulse anim
- `dialController.functionCycled()` → update function info, play cycle anim, show function dots briefly
- `dialController.functionChanged()` → update function name/icon/unit
- `dialController.adjustingChanged()` → control hide timer

The back circle gauge canvas draws:
```javascript
// 20 segments, each 18° apart, with 3° gap between
var segCount = 20;
var segAngle = (Math.PI * 2) / segCount;
var gap = segAngle * 0.15;
var startOffset = -Math.PI / 2;
var filledSegs = Math.round(normalizedValue * segCount);

for (var i = 0; i < segCount; i++) {
    var a1 = startOffset + i * segAngle + gap / 2;
    var a2 = a1 + segAngle - gap;
    ctx.beginPath();
    ctx.arc(cx, cy, radius, a1, a2);
    ctx.strokeStyle = (i < filledSegs) ? accentColor : dimColor;
    ctx.lineWidth = thickness;
    ctx.lineCap = "round";
    ctx.stroke();
}
```

- [ ] **Step 2: Build and verify**

Run: `cmake --build build --target openwheel-gadget 2>&1`
Expected: compiles (QRC auto-updates)

- [ ] **Step 3: Manual test**

Kill and restart gadget. Rotate dial. Verify:
- Overlay appears at bottom center
- Two-circle 3D look (front circle with shadow over gauge ring)
- Gauge segments fill/unfill matching value
- Spring animation on show, shrink on hide
- Pulse on rotation
- Function name/icon/value displayed

- [ ] **Step 4: Commit**

```bash
git add openwheel-gadget/src/ui/OverlayWindow.qml
git commit -m "feat: 3D two-circle overlay with segmented gauge and animations"
```

---

### Task 4: Wire Real Values for All Functions

**Files:**
- Modify: `openwheel-gadget/src/core/actionexecutor.cpp`
- Modify: `openwheel-gadget/src/core/dialcontroller.cpp`

- [ ] **Step 1: Read initial system values on function switch**

Add method to `ActionExecutor` to query current value:

In `actionexecutor.h`:
```cpp
void queryCurrentValue(const QString &keys);
```

In `actionexecutor.cpp`:
```cpp
void ActionExecutor::queryCurrentValue(const QString &keys)
{
    if (keys == QStringLiteral("XF86AudioRaiseVolume") ||
        keys == QStringLiteral("XF86AudioLowerVolume")) {
        QProcess proc;
        proc.start(QStringLiteral("wpctl"),
            {QStringLiteral("get-volume"), QStringLiteral("@DEFAULT_AUDIO_SINK@")});
        proc.waitForFinished(500);
        QString output = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        QStringList parts = output.split(QLatin1Char(' '));
        if (parts.size() >= 2) {
            qreal vol = parts.last().toDouble() * 100.0;
            Q_EMIT systemValueChanged(vol, 0.0, 100.0);
        }
    } else if (keys == QStringLiteral("XF86MonBrightnessUp") ||
               keys == QStringLiteral("XF86MonBrightnessDown")) {
        auto bus = QDBusConnection::sessionBus();
        QString service = QStringLiteral("org.kde.Solid.PowerManagement");
        QString path = QStringLiteral("/org/kde/Solid/PowerManagement/Actions/BrightnessControl");
        QString iface = QStringLiteral("org.kde.Solid.PowerManagement.Actions.BrightnessControl");

        QDBusReply<int> current = bus.call(
            QDBusMessage::createMethodCall(service, path, iface, QStringLiteral("brightness")));
        QDBusReply<int> max = bus.call(
            QDBusMessage::createMethodCall(service, path, iface, QStringLiteral("brightnessMax")));

        if (current.isValid() && max.isValid() && max.value() > 0) {
            qreal pct = static_cast<qreal>(current.value()) / max.value() * 100.0;
            Q_EMIT systemValueChanged(pct, 0.0, 100.0);
        }
    }
    // Zoom and Scroll have no queryable value
}
```

- [ ] **Step 2: Call queryCurrentValue when function switches**

In `dialcontroller.cpp`, at the end of `updateCurrentFunction()`, after `setCurrentActions`:

```cpp
// Query real system value for the new function
m_actionExecutor->queryCurrentValue(func.clockwiseAction.keys);
```

- [ ] **Step 3: Build and verify**

Run: `cmake --build build --target openwheel-gadget 2>&1`
Expected: compiles cleanly

- [ ] **Step 4: Commit**

```bash
git add openwheel-gadget/src/core/actionexecutor.h openwheel-gadget/src/core/actionexecutor.cpp openwheel-gadget/src/core/dialcontroller.cpp
git commit -m "feat: query real system values on function switch"
```

---

### Task 5: Update Profile Value Ranges

**Files:**
- Modify: `profiles/system-default.json`

- [ ] **Step 1: Set proper value ranges**

Full updated `system-default.json`:

```json
{
  "id": "system-default",
  "displayName": "System",
  "processNames": [],
  "isDefault": true,
  "functions": [
    {
      "id": "volume",
      "label": "Volume",
      "iconName": "audio-volume-high",
      "type": "continuous",
      "minValue": 0,
      "maxValue": 100,
      "unit": "%",
      "clockwiseAction": {
        "type": "keyboard",
        "keys": "XF86AudioRaiseVolume",
        "rotationThreshold": 5
      },
      "counterClockwiseAction": {
        "type": "keyboard",
        "keys": "XF86AudioLowerVolume",
        "rotationThreshold": 5
      }
    },
    {
      "id": "brightness",
      "label": "Brightness",
      "iconName": "display-brightness",
      "type": "continuous",
      "minValue": 0,
      "maxValue": 100,
      "unit": "%",
      "clockwiseAction": {
        "type": "keyboard",
        "keys": "XF86MonBrightnessUp",
        "rotationThreshold": 5
      },
      "counterClockwiseAction": {
        "type": "keyboard",
        "keys": "XF86MonBrightnessDown",
        "rotationThreshold": 5
      }
    },
    {
      "id": "zoom",
      "label": "Zoom",
      "iconName": "zoom-in",
      "type": "discrete",
      "clockwiseAction": {
        "type": "keyboard",
        "keys": "ZoomIn",
        "rotationThreshold": 5
      },
      "counterClockwiseAction": {
        "type": "keyboard",
        "keys": "ZoomOut",
        "rotationThreshold": 5
      }
    },
    {
      "id": "scroll",
      "label": "Scroll",
      "iconName": "view-scroll-vertical",
      "type": "discrete",
      "clockwiseAction": {
        "type": "mouseScroll",
        "delta": -3,
        "rotationThreshold": 3
      },
      "counterClockwiseAction": {
        "type": "mouseScroll",
        "delta": 3,
        "rotationThreshold": 3
      }
    }
  ],
  "defaultMenuLayout": ["volume", "brightness", "zoom", "scroll"]
}
```

Key changes:
- Volume & Brightness: `continuous`, 0-100%
- Zoom & Scroll: `discrete` (no value range — gauge ring hidden for these)
- Icon names use freedesktop icon names that Kirigami.Icon can resolve

- [ ] **Step 2: Commit**

```bash
git add profiles/system-default.json
git commit -m "feat: proper value ranges and icon names for system profile"
```

---

### Task 6: Integration Test

- [ ] **Step 1: Rebuild everything**

```bash
cmake --build build 2>&1
```

- [ ] **Step 2: Run unit tests**

```bash
cd build && ctest --output-on-failure
```

- [ ] **Step 3: Manual integration test**

Start daemon + gadget:
```bash
sudo chmod a+r /dev/hidraw1
./build/bin/asus_wheel &
XDG_DATA_DIRS="/tmp/openwheel-install/share:${XDG_DATA_DIRS}" ./build/openwheel-gadget/openwheel-gadget
```

Verify:
1. Rotate → overlay appears with 3D two-circle design, volume gauge fills
2. Keep rotating → volume changes, gauge updates in real time
3. Double-click → switches to Brightness, gauge shows current brightness %
4. Double-click → switches to Zoom, gauge ring hidden (discrete)
5. Double-click → switches to Scroll, gauge ring hidden (discrete)
6. Double-click → back to Volume
7. Overlay auto-hides after 2s of no rotation
8. Animations: spring-in on show, pulse on tick, shrink-out on hide

- [ ] **Step 4: Final commit if any fixups**

```bash
git add -A && git commit -m "fix: integration test fixups"
```
