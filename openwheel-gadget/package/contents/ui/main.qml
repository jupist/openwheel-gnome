import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami
import org.kde.plasma.plasmoid 2.0

PlasmoidItem {
    id: root

    width: Kirigami.Units.gridUnit * 16
    height: width

    property int modeIndex: 0
    property int currentValue: 50
    property int minValue: 0
    property int maxValue: 100
    property bool overlayVisible: true

    readonly property real normalizedValue: (currentValue - minValue) / (maxValue - minValue)

    ListModel {
        id: modes
        ListElement { name: "Volume"; unit: "%"; accent: "#ff7b6f"; icon: "audio-volume-high" }
        ListElement { name: "Brightness"; unit: "%"; accent: "#f6d365"; icon: "display-brightness" }
        ListElement { name: "Scroll"; unit: "%"; accent: "#5bc0eb"; icon: "input-mouse" }
    }

    function clampValue(value) {
        return Math.max(minValue, Math.min(maxValue, value));
    }

    function showOverlay() {
        overlayVisible = true;
        hideTimer.restart();
    }

    function stepValue(delta) {
        currentValue = clampValue(currentValue + delta);
        showOverlay();
        dialCanvas.requestPaint();
    }

    function nextMode() {
        modeIndex = (modeIndex + 1) % modes.count;
        showOverlay();
        dialCanvas.requestPaint();
    }

    Component.onCompleted: showOverlay()

    Timer {
        id: hideTimer
        interval: 1400
        onTriggered: overlayVisible = false
    }

    WheelHandler {
        id: wheelHandler
        target: root
        onWheel: {
            stepValue(wheel.angleDelta.y > 0 ? 2 : -2);
        }
    }

    TapHandler {
        acceptedButtons: Qt.LeftButton
        onTapped: nextMode()
    }

    Rectangle {
        id: background
        anchors.fill: parent
        radius: width / 2
        color: Kirigami.Theme.backgroundColor
        border.color: Qt.darker(Kirigami.Theme.backgroundColor, 1.1)
        border.width: 1
    }

    Canvas {
        id: dialCanvas
        anchors.fill: parent
        renderTarget: Canvas.Image
        onPaint: {
            var ctx = getContext("2d");
            ctx.reset();
            ctx.clearRect(0, 0, width, height);

            var centerX = width / 2;
            var centerY = height / 2;
            var ringThickness = Math.max(10, width * 0.08);
            var radius = (Math.min(width, height) / 2) - ringThickness - Kirigami.Units.smallSpacing;

            ctx.lineWidth = ringThickness;
            ctx.lineCap = "round";

            ctx.beginPath();
            ctx.strokeStyle = Qt.rgba(0.2, 0.2, 0.2, 0.35);
            ctx.arc(centerX, centerY, radius, 0, Math.PI * 2, false);
            ctx.stroke();

            var startAngle = -Math.PI / 2;
            var endAngle = startAngle + (Math.PI * 2 * normalizedValue);
            var accentColor = modes.get(modeIndex).accent;

            ctx.beginPath();
            ctx.strokeStyle = accentColor;
            ctx.arc(centerX, centerY, radius, startAngle, endAngle, false);
            ctx.stroke();

            ctx.lineWidth = Math.max(2, ringThickness * 0.25);
            ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.15);
            for (var i = 0; i < 12; i++) {
                var tickAngle = startAngle + (Math.PI * 2 * i / 12);
                var inner = radius - ringThickness * 0.55;
                var outer = radius - ringThickness * 0.1;
                ctx.beginPath();
                ctx.moveTo(centerX + Math.cos(tickAngle) * inner, centerY + Math.sin(tickAngle) * inner);
                ctx.lineTo(centerX + Math.cos(tickAngle) * outer, centerY + Math.sin(tickAngle) * outer);
                ctx.stroke();
            }
        }
    }

    Rectangle {
        id: innerSurface
        width: parent.width * 0.58
        height: width
        anchors.centerIn: parent
        radius: width / 2
        color: Qt.darker(Kirigami.Theme.backgroundColor, 1.08)
        border.color: Qt.rgba(1, 1, 1, 0.06)
    }

    ColumnLayout {
        anchors.centerIn: parent
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Icon {
            Layout.alignment: Qt.AlignHCenter
            width: Kirigami.Units.iconSizes.medium
            height: width
            source: modes.get(modeIndex).icon
            color: Kirigami.Theme.textColor
        }

        Kirigami.Heading {
            Layout.alignment: Qt.AlignHCenter
            level: 2
            text: modes.get(modeIndex).name
            color: Kirigami.Theme.textColor
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: currentValue + modes.get(modeIndex).unit
            font.pixelSize: Kirigami.Units.gridUnit * 1.4
            font.weight: Font.DemiBold
            color: Kirigami.Theme.textColor
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            text: i18n("Global")
            color: Qt.rgba(1, 1, 1, 0.6)
            opacity: overlayVisible ? 1 : 0
            Behavior on opacity {
                NumberAnimation { duration: 160 }
            }
        }
    }

    Rectangle {
        id: overlayPulse
        anchors.fill: parent
        radius: width / 2
        color: Qt.rgba(1, 1, 1, 0.06)
        opacity: overlayVisible ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: 220 }
        }
    }

    Text {
        id: hintText
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Kirigami.Units.smallSpacing
        text: i18n("Scroll to adjust • Click to change mode")
        font.pixelSize: Kirigami.Units.smallSpacing * 1.6
        color: Qt.rgba(1, 1, 1, 0.5)
        opacity: overlayVisible ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
    }

    Connections {
        target: root
        function onModeIndexChanged() {
            dialCanvas.requestPaint();
        }

        function onCurrentValueChanged() {
            dialCanvas.requestPaint();
        }
    }
}
