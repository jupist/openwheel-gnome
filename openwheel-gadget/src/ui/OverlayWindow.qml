import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import org.kde.kirigami as Kirigami

Window {
    id: overlay
    width: 380
    height: 380
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool | Qt.WindowTransparentForInput
    visible: false

    x: (Screen.width - width) / 2
    y: Screen.height - height - 100

    // ── Properties ──────────────────────────────────────────────────────
    property bool overlayActive: dialController.active
    property string functionName: ""
    property string iconName: ""
    property string unitText: ""
    property real currentValue: 0
    property real minVal: 0
    property real maxVal: 100
    property int selectedIndex: dialController.selectedFunctionIndex
    property bool isDiscrete: false
    property bool showDots: false

    property real normalizedValue: {
        if (isDiscrete) return 0;
        var range = maxVal - minVal;
        if (range <= 0) return 0;
        return Math.max(0, Math.min(1, (currentValue - minVal) / range));
    }

    Behavior on normalizedValue {
        NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
    }

    // ── Connections to DialController ───────────────────────────────────
    Connections {
        target: dialController

        function onRotationTick() {
            pulseAnim.start();
            hideTimer.restart();
            if (!overlay.visible) {
                overlay.visible = true;
                showAnim.start();
            }
        }

        function onValueChanged(value) {
            currentValue = value;
            gaugeCanvas.requestPaint();
        }

        function onFunctionChanged() {
            updateFunctionInfo();
            gaugeCanvas.requestPaint();
        }

        function onFunctionCycled() {
            cycleAnim.start();
            updateFunctionInfo();
            showDots = true;
            dotsTimer.restart();
            gaugeCanvas.requestPaint();
        }

        function onAdjustingChanged() {
            if (dialController.isAdjusting) {
                hideTimer.stop();
                if (!overlay.visible) {
                    overlay.visible = true;
                    showAnim.start();
                }
            } else {
                hideTimer.restart();
            }
        }
    }

    // ── Helper functions ────────────────────────────────────────────────
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

    // ── Timers ──────────────────────────────────────────────────────────
    Timer {
        id: hideTimer
        interval: 2500
        onTriggered: hideAnim.start()
    }

    Timer {
        id: dotsTimer
        interval: 1500
        onTriggered: showDots = false
    }

    // ── Show animation ──────────────────────────────────────────────────
    ParallelAnimation {
        id: showAnim
        NumberAnimation {
            target: content
            property: "scale"
            from: 0.85; to: 1.0
            duration: 300
            easing.type: Easing.OutBack
        }
        NumberAnimation {
            target: content
            property: "opacity"
            from: 0; to: 1
            duration: 150
            easing.type: Easing.OutQuad
        }
    }

    // ── Hide animation ──────────────────────────────────────────────────
    ParallelAnimation {
        id: hideAnim
        NumberAnimation {
            target: content
            property: "scale"
            from: 1.0; to: 0.92
            duration: 150
            easing.type: Easing.InQuad
        }
        NumberAnimation {
            target: content
            property: "opacity"
            from: 1; to: 0
            duration: 200
            easing.type: Easing.InQuad
        }
        onFinished: {
            overlay.visible = false;
            content.scale = 0.85;
        }
    }

    // ── Rotation pulse animation (front circle) ─────────────────────────
    SequentialAnimation {
        id: pulseAnim
        NumberAnimation {
            target: frontCircle
            property: "scale"
            from: 1.0; to: 1.025
            duration: 80
            easing.type: Easing.OutQuad
        }
        NumberAnimation {
            target: frontCircle
            property: "scale"
            from: 1.025; to: 1.0
            duration: 120
            easing.type: Easing.InQuad
        }
    }

    // ── Function cycle animation (front circle bounce) ──────────────────
    SequentialAnimation {
        id: cycleAnim
        NumberAnimation {
            target: frontCircle
            property: "scale"
            from: 1.0; to: 0.95
            duration: 125
            easing.type: Easing.InOutQuad
        }
        NumberAnimation {
            target: frontCircle
            property: "scale"
            from: 0.95; to: 1.0
            duration: 125
            easing.type: Easing.OutBack
        }
    }

    // ── Main content container ──────────────────────────────────────────
    Item {
        id: content
        anchors.fill: parent
        scale: 0.85
        opacity: 0

        // ── Back circle (gauge ring) ────────────────────────────────────
        Item {
            id: backCircle
            width: 340
            height: 340
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 2

            // Dim background circle
            Rectangle {
                anchors.fill: parent
                radius: width / 2
                color: Qt.darker(Kirigami.Theme.backgroundColor, 1.3)
            }

            // Segmented gauge arc
            Canvas {
                id: gaugeCanvas
                anchors.fill: parent
                visible: !isDiscrete

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.reset();
                    ctx.clearRect(0, 0, width, height);

                    var cx = width / 2;
                    var cy = height / 2;
                    var lineWidth = 14;
                    var r = (Math.min(width, height) / 2) - lineWidth - 4;

                    var totalSegments = 20;
                    var gapAngle = 0.04; // radians gap between segments
                    var totalArc = Math.PI * 2;
                    var segmentArc = (totalArc - totalSegments * gapAngle) / totalSegments;

                    var filledCount = Math.round(normalizedValue * totalSegments);
                    filledCount = Math.max(0, Math.min(totalSegments, filledCount));

                    var startAngle = -Math.PI / 2; // top of circle

                    ctx.lineWidth = lineWidth;
                    ctx.lineCap = "round";

                    for (var i = 0; i < totalSegments; i++) {
                        var segStart = startAngle + i * (segmentArc + gapAngle);
                        var segEnd = segStart + segmentArc;

                        ctx.beginPath();
                        if (i < filledCount) {
                            ctx.strokeStyle = Kirigami.Theme.highlightColor;
                        } else {
                            ctx.strokeStyle = Qt.darker(Kirigami.Theme.backgroundColor, 1.3);
                        }
                        ctx.arc(cx, cy, r, segStart, segEnd);
                        ctx.stroke();
                    }
                }
            }
        }

        // ── Front circle drop shadow ────────────────────────────────────
        Rectangle {
            id: frontShadow
            width: 268
            height: 268
            radius: width / 2
            color: "#50000000"
            anchors.centerIn: parent
            anchors.verticalCenterOffset: 6
        }

        // ── Front circle (info display) ─────────────────────────────────
        Rectangle {
            id: frontCircle
            width: 260
            height: 260
            radius: width / 2
            color: Kirigami.Theme.backgroundColor
            border.color: Kirigami.Theme.separatorColor
            border.width: 1
            anchors.centerIn: parent

            // Center content
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 4

                // Function icon
                Kirigami.Icon {
                    Layout.alignment: Qt.AlignHCenter
                    source: iconName
                    implicitWidth: 36
                    implicitHeight: 36
                    color: Kirigami.Theme.textColor
                }

                // Function name
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: functionName
                    font.pixelSize: 15
                    font.weight: Font.Medium
                    color: Kirigami.Theme.textColor
                }

                // Value text
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: isDiscrete ? "\u2014" : Math.round(currentValue) + unitText
                    font.pixelSize: 30
                    font.weight: Font.Bold
                    color: Kirigami.Theme.highlightColor
                }

                // Profile name
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: dialController.currentProfileName
                    font.pixelSize: 10
                    color: Kirigami.Theme.disabledTextColor
                }
            }

            // ── Function selector dots ──────────────────────────────────
            Row {
                id: functionDots
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 20
                spacing: 6
                visible: showDots
                opacity: showDots ? 1.0 : 0.0

                Behavior on opacity {
                    NumberAnimation { duration: 200 }
                }

                Repeater {
                    model: dialController.currentFunctions
                    delegate: Rectangle {
                        width: 6
                        height: 6
                        radius: 3
                        color: index === dialController.selectedFunctionIndex
                               ? Kirigami.Theme.highlightColor
                               : Kirigami.Theme.disabledTextColor
                    }
                }
            }
        }
    }
}
