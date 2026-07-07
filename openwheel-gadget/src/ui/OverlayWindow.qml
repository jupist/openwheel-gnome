import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import "settings"

// OverlayWindow — frameless, translucent HUD shown when the dial is active.
//
// Two pages share the same window:
//   - mainView  : normal function/value display (existing behaviour)
//   - pickerView: profile selection triggered by a long press on the dial
//
// Kirigami has been replaced by plain Qt palette colours so this renders
// correctly on GNOME (Fusion style) and KDE (Fusion or Breeze) alike.

Window {
    id: overlay
    width: 380
    height: 380
    color: "transparent"
    // Qt.Tool + WindowDoesNotAcceptFocus: creates a standalone top-level that
    // does not steal keyboard focus. Qt.ToolTip requires a transient parent
    // (popup) which we don't have, so it fails on Wayland.
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool
         | Qt.WindowDoesNotAcceptFocus
    visible: false

    x: (Screen.width - width) / 2
    y: Screen.height - height - 100

    // ── Dark monospace theme colours ─────────────────────────────────────
    readonly property color bgColor:       "#0d0d0d"
    readonly property color fgColor:       "#e8e8e8"
    readonly property color accentColor:   "#ffffff"
    readonly property color mutedColor:    "#888888"
    readonly property color surfaceColor:  "#161616"
    readonly property color dimBgColor:    "#121212"
    readonly property string monoFont:     "monospace"

    // ── Function display state ───────────────────────────────────────────
    property string functionName: ""
    property string iconName: ""
    property string unitText: ""
    property real currentValue: 0
    property real minVal: 0
    property real maxVal: 100
    property int selectedIndex: dialController.selectedFunctionIndex
    property bool isDiscrete: false
    property bool showDots: false
    // Track info from MPRIS (empty when not in music mode or no player running)
    property string trackTitle:  dialController.mediaTitle
    property string trackArtist: dialController.mediaArtist

    // True while the overlay is hidden for a focus-escape (zoom injection).
    // Suppresses rotation ticks from re-showing the overlay mid-escape.
    property bool inFocusEscape: false

    // True when the currently selected function needs the overlay to stay
    // hidden during use (e.g. zoom — keystrokes must go to the focused app).
    readonly property bool currentFunctionNeedsEscape: {
        var funcs = dialController.currentFunctions;
        var idx   = dialController.selectedFunctionIndex;
        return (funcs.length > 0 && idx < funcs.length)
               ? (funcs[idx].needsFocusEscape === true)
               : false;
    }

    property real normalizedValue: {
        if (isDiscrete) return 0;
        var range = maxVal - minVal;
        if (range <= 0) return 0;
        return Math.max(0, Math.min(1, (currentValue - minVal) / range));
    }

    Behavior on normalizedValue {
        SmoothedAnimation { duration: 180 }
    }

    // Drive canvas repaints at animation frame-rate as normalizedValue animates
    onNormalizedValueChanged: gaugeCanvas.requestPaint()

    // ── Settings window (persistent — just show/hide, never recreate) ───
    SettingsWindow {
        id: settingsWin
        visible: false
    }

    // ── Connections to DialController ────────────────────────────────────
    Connections {
        target: dialController

        function onSettingsRequested() {
            settingsWin.visible = true
            settingsWin.raise()
            settingsWin.requestActivate()
        }

        function onRotationTick() {
            if (dialController.pickerActive) return;
            pulseAnim.start();
            hideTimer.restart();
            if (!overlay.visible) {
                overlay.visible = true;
                showAnim.start();
            }
            overlay.raise();
        }

        function onValueChanged(value) {
            currentValue = value;
        }

        function onFunctionChanged() {
            updateFunctionInfo();
        }

        function onFunctionCycled() {
            if (dialController.pickerActive) return;
            cycleAnim.start();
            updateFunctionInfo();
            showDots = true;
            dotsTimer.restart();
        }

        function onAdjustingChanged() {
            if (dialController.pickerActive) return;
            if (dialController.isAdjusting) {
                hideTimer.stop();
                if (!overlay.visible) {
                    overlay.visible = true;
                    showAnim.start();
                }
                overlay.raise();
            } else {
                hideTimer.restart();
            }
        }

        function onPickerActiveChanged(active) {
            if (active !== 0) {
                pickerView.displayMode = active;
                hideTimer.stop();
                overlay.visible = true;
                pickerShowAnim.start();
            } else {
                pickerHideAnim.start();
            }
        }

        function onPickerIndexChanged(index) {
            pickerList.currentIndex = index;
        }
    }

    // ── Helper functions ─────────────────────────────────────────────────
    function updateFunctionInfo() {
        var funcs = dialController.currentFunctions;
        var idx = dialController.selectedFunctionIndex;
        if (funcs.length > 0 && idx < funcs.length) {
            var f = funcs[idx];
            functionName = f.label || "";
            iconName = f.iconName || "";
            unitText = f.unit || "";
            isDiscrete = (f.type === "discrete");
            if (!isDiscrete && f.minValue !== undefined && f.maxValue !== undefined) {
                minVal = f.minValue;
                maxVal = f.maxValue;
            } else {
                minVal = 0;
                maxVal = 100;
            }
        }
    }

    Component.onCompleted: updateFunctionInfo()

    // ── Timers ────────────────────────────────────────────────────────────
    Timer { id: hideTimer; interval: 2500; onTriggered: hideAnim.start() }
    Timer { id: dotsTimer; interval: 1500; onTriggered: showDots = false }

    // ── Main view animations ──────────────────────────────────────────────
    ParallelAnimation {
        id: showAnim
        NumberAnimation { target: content; property: "scale";   from: 0.85; to: 1.0; duration: 300; easing.type: Easing.OutBack }
        NumberAnimation { target: content; property: "opacity"; from: 0;    to: 1;   duration: 150; easing.type: Easing.OutQuad }
    }
    ParallelAnimation {
        id: hideAnim
        NumberAnimation { target: content; property: "scale";   from: 1.0;  to: 0.92; duration: 150; easing.type: Easing.InQuad }
        NumberAnimation { target: content; property: "opacity"; from: 1;    to: 0;    duration: 200; easing.type: Easing.InQuad }
        onFinished: { overlay.visible = false; content.scale = 0.85; }
    }
    SequentialAnimation {
        id: pulseAnim
        NumberAnimation { target: frontCircle; property: "scale"; from: 1.0; to: 1.025; duration: 80;  easing.type: Easing.OutQuad }
        NumberAnimation { target: frontCircle; property: "scale"; from: 1.025; to: 1.0; duration: 120; easing.type: Easing.InQuad }
    }
    SequentialAnimation {
        id: cycleAnim
        NumberAnimation { target: frontCircle; property: "scale"; from: 1.0; to: 0.95; duration: 125; easing.type: Easing.InOutQuad }
        NumberAnimation { target: frontCircle; property: "scale"; from: 0.95; to: 1.0; duration: 125; easing.type: Easing.OutBack }
    }

    // ── Profile picker animations ─────────────────────────────────────────
    ParallelAnimation {
        id: pickerShowAnim
        NumberAnimation { target: pickerView; property: "scale";   from: 0.85; to: 1.0; duration: 280; easing.type: Easing.OutBack }
        NumberAnimation { target: pickerView; property: "opacity"; from: 0;    to: 1;   duration: 160; easing.type: Easing.OutQuad }
    }
    ParallelAnimation {
        id: pickerHideAnim
        NumberAnimation { target: pickerView; property: "scale";   from: 1.0;  to: 0.9; duration: 160; easing.type: Easing.InQuad }
        NumberAnimation { target: pickerView; property: "opacity"; from: 1;    to: 0;   duration: 180; easing.type: Easing.InQuad }
        onFinished: { pickerView.scale = 0.85; if (!dialController.pickerActive) overlay.visible = false; }
    }

    // ════════════════════════════════════════════════════════════════════
    // MAIN VIEW — function display
    // ════════════════════════════════════════════════════════════════════
    Item {
        id: content
        anchors.fill: parent
        scale: 0.85
        opacity: 0
        visible: !dialController.pickerActive || pickerView.opacity < 0.05

        // Back circle (gauge ring)
        Item {
            id: backCircle
            width: 340; height: 340
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 2

            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: overlay.dimBgColor
            }

            Canvas {
                id: gaugeCanvas
                anchors.fill: parent
                visible: !isDiscrete

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.clearRect(0, 0, width, height);

                    var cx = width / 2, cy = height / 2;
                    var lineWidth = 14;
                    var r = (Math.min(width, height) / 2) - lineWidth - 4;
                    var totalSegments = 20;
                    var gapAngle = 0.04;
                    var totalArc = Math.PI * 2;
                    var segmentArc = (totalArc - totalSegments * gapAngle) / totalSegments;
                    var filledCount = Math.max(0, Math.min(totalSegments, Math.round(normalizedValue * totalSegments)));
                    var startAngle = -Math.PI / 2;

                    ctx.lineWidth = lineWidth;
                    ctx.lineCap = "round";

                    for (var i = 0; i < totalSegments; i++) {
                        var segStart = startAngle + i * (segmentArc + gapAngle);
                        ctx.beginPath();
                        ctx.strokeStyle = (i < filledCount) ? overlay.accentColor : overlay.dimBgColor;
                        ctx.arc(cx, cy, r, segStart, segStart + segmentArc);
                        ctx.stroke();
                    }
                }
            }
        }

        // Front circle drop-shadow
        Rectangle {
            width: 268; height: 268
            radius: width / 2
            color: "#50000000"
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 6
        }

        // Front circle — info display
        Rectangle {
            id: frontCircle
            width: 260; height: 260
            radius: width / 2
            color: overlay.surfaceColor
            border.color: "#2a2a2a"
            border.width: 1
            anchors.centerIn: parent

            // Main content — only function name + value, so it centres cleanly
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 6

                // Icon — QIcon::fromTheme works on both GNOME and KDE.
                Image {
                    Layout.alignment: Qt.AlignHCenter
                    source: iconName ? "image://icon/" + iconName : ""
                    width: 36; height: 36
                    fillMode: Image.PreserveAspectFit
                    visible: iconName !== "" && status === Image.Ready
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: functionName
                    font.family: overlay.monoFont
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    color: overlay.fgColor
                }

                // Value / track name display
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: 200
                    // For discrete media functions: show track title if available, else em-dash.
                    // For continuous functions: show the numeric value + unit.
                    text: isDiscrete
                          ? (trackTitle !== "" ? trackTitle : "\u2014")
                          : Math.round(currentValue) + unitText
                    font.family: overlay.monoFont
                    font.pixelSize: isDiscrete && trackTitle !== "" ? 13 : 30
                    font.weight: Font.Bold
                    color: overlay.accentColor
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    maximumLineCount: 2
                    elide: Text.ElideRight
                }

                // Artist name — only shown when we have track info
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    Layout.maximumWidth: 200
                    visible: isDiscrete && trackArtist !== ""
                    text: trackArtist
                    font.family: overlay.monoFont
                    font.pixelSize: 11
                    font.weight: Font.Normal
                    color: overlay.mutedColor
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    maximumLineCount: 1
                    elide: Text.ElideRight
                }
            }

            // Profile name sits at the bottom of the circle, separate from the
            // centered content so it doesn't pull the visual centre of gravity down
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 38
                text: dialController.currentProfileName
                font.family: overlay.monoFont
                font.pixelSize: 10
                color: overlay.mutedColor
            }

            // Function selector dots
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 22
                spacing: 6
                visible: showDots
                opacity: showDots ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: 200 } }

                Repeater {
                    model: dialController.currentFunctions
                    delegate: Rectangle {
                        width: 6; height: 6; radius: 3
                        color: index === dialController.selectedFunctionIndex
                               ? overlay.accentColor : overlay.mutedColor
                    }
                }
            }
        }
    }

    // ════════════════════════════════════════════════════════════════════
    // PROFILE PICKER — shown on long press
    // ════════════════════════════════════════════════════════════════════
    Item {
        id: pickerView
        property int displayMode: 1
        anchors.fill: parent
        scale: 0.85
        opacity: 0
        visible: opacity > 0.01

        // Dark background circle
        Rectangle {
            width: 340; height: 340
            anchors.centerIn: parent
            radius: width / 2
            color: overlay.dimBgColor
        }

        // White inner circle
        Rectangle {
            width: 260; height: 260
            anchors.centerIn: parent
            radius: width / 2
            color: overlay.surfaceColor
            border.color: overlay.accentColor
            border.width: 2

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 20
                spacing: 6

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: pickerView.displayMode === 2 ? "Select Action" : "Select Profile"
                    font.family: overlay.monoFont
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: overlay.mutedColor
                }

                // Profile / Action list — keep current item centred in viewport
                ListView {
                    id: pickerList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: pickerView.displayMode === 2 ? dialController.currentFunctions : dialController.availableProfiles
                    clip: true
                    currentIndex: dialController.pickerIndex
                    highlightMoveDuration: 120
                    // StrictlyEnforceRange pins the selected item at the centre
                    highlightRangeMode: ListView.StrictlyEnforceRange
                    preferredHighlightBegin: height / 2 - 16
                    preferredHighlightEnd:   height / 2 + 16

                    highlight: Rectangle {
                        color: overlay.accentColor
                        radius: 4
                        opacity: 0.25
                    }

                    delegate: Item {
                        width: pickerList.width
                        height: modelData.id === "__settings__" ? 38 : 32

                        // Thin divider above the Settings entry
                        Rectangle {
                            visible: modelData.id === "__settings__"
                            anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                            anchors.leftMargin: 12; anchors.rightMargin: 12
                            height: 1
                            color: overlay.mutedColor
                            opacity: 0.4
                        }

                        Text {
                            width: parent.width
                            height: parent.height
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            topPadding: modelData.id === "__settings__" ? 3 : 0
                            text: modelData.name || modelData.label || ""
                            font.family: overlay.monoFont
                            font.pixelSize: modelData.id === "__settings__" ? 11 : 13
                            font.weight: index === pickerList.currentIndex ? Font.Bold : Font.Normal
                            color: index === pickerList.currentIndex
                                   ? overlay.accentColor
                                   : (modelData.id === "__settings__" ? overlay.mutedColor : overlay.fgColor)
                        }
                    }
                }

                // Hint text
                Text {
                    Layout.fillWidth: true
                    text: "Rotate to browse · Press to confirm"
                    font.family: overlay.monoFont
                    font.pixelSize: 9
                    color: overlay.mutedColor
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                }
            }
        }
    }
}
