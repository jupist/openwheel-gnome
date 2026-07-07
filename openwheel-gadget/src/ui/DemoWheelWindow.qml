// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

Window {
    id: demoWin
    width: Screen.width
    height: Screen.height
    color: "transparent"
    flags: Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.Tool | Qt.WindowDoesNotAcceptFocus | Qt.WindowTransparentForInput | Qt.BypassWindowManagerHint
    visible: false

    property real wheelAngle: 0
    property bool isTurning: false
    property bool isPressed: false

    Connections {
        target: dialController

        function onRawRotationTick(delta) {
            demoWin.wheelAngle += delta * 15
            demoWin.isTurning = true
            if (dialController.showDemoWheel !== 0) {
                if (!demoWin.visible) {
                    demoWin.visible = true
                    demoWin.raise()
                }
            }
            keepAliveTimer.restart()
        }

        function onButtonHeldChanged() {
            if (dialController.buttonHeld !== 0) {
                demoWin.isPressed = true
                if (dialController.showDemoWheel !== 0) {
                    if (!demoWin.visible) {
                        demoWin.visible = true
                        demoWin.raise()
                    }
                }
                keepAliveTimer.restart()
            } else {
                demoWin.isPressed = false
                keepAliveTimer.restart()
            }
        }

        function onShowDemoWheelChanged() {
            if (dialController.showDemoWheel === 0) {
                demoWin.visible = false
            } else if (demoWin.isTurning || demoWin.isPressed) {
                demoWin.visible = true
                demoWin.raise()
            }
        }
    }

    Timer {
        id: keepAliveTimer
        interval: 500
        onTriggered: {
            demoWin.isTurning = false
            if (dialController.buttonHeld === 0) {
                demoWin.isPressed = false
            }
        }
    }

    // ── Wheel Assembly (Locked to bottom-left corner of the monitor) ─────────
    Item {
        id: wheelAssembly
        width: 110
        height: 110
        x: 30
        y: Screen.height - height - 80
        visible: isTurning || isPressed || keepAliveTimer.running

        // Subtle glow behind the wheel when active
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: 52
            color: "transparent"
            border.color: (isTurning || isPressed) ? "#40ffffff" : "transparent"
            border.width: 3
            opacity: (isTurning || isPressed) ? 1 : 0
            Behavior on opacity { NumberAnimation { duration: 120 } }
        }

        // Outermost ring — Entire border lights up white when turning or pressed!
        Rectangle {
            anchors.fill: parent
            radius: 48
            color: "#161616"
            border.color: (isTurning || isPressed) ? "#ffffff" : "#383838"
            border.width: (isTurning || isPressed) ? 2.5 : 1.5
            Behavior on border.color { ColorAnimation { duration: 100 } }
            Behavior on border.width { NumberAnimation { duration: 100 } }

            // Fixed outer tick marks around the ring
            Repeater {
                model: 12
                Rectangle {
                    width: 2
                    height: 6
                    color: (isTurning || isPressed) ? "#888888" : "#444444"
                    x: 47
                    y: 2
                    transform: Rotation {
                        origin.x: 1
                        origin.y: 46
                        angle: index * 30
                    }
                    Behavior on color { ColorAnimation { duration: 100 } }
                }
            }
        }

        // Rotating disc (no border lighting up here, just rotates the marker)
        Item {
            id: rotatingDisc
            anchors.centerIn: parent
            width: 76
            height: 76
            rotation: demoWin.wheelAngle
            Behavior on rotation {
                SmoothedAnimation { velocity: 800 }
            }

            Rectangle {
                anchors.fill: parent
                radius: 38
                color: "#111111"
                border.width: 0

                // Rotation indicator marker at top edge
                Rectangle {
                    width: 6
                    height: 10
                    radius: 3
                    color: isTurning ? "#ffffff" : "#888888"
                    anchors.top: parent.top
                    anchors.topMargin: 4
                    anchors.horizontalCenter: parent.horizontalCenter
                    Behavior on color { ColorAnimation { duration: 100 } }
                }
            }
        }

        // Center physical button
        Rectangle {
            id: centerButton
            width: 44
            height: 44
            radius: 22
            anchors.centerIn: parent
            color: isPressed ? "#ffffff" : "#242424"
            border.color: isPressed ? "#ffffff" : "#555555"
            border.width: isPressed ? 2 : 1
            scale: isPressed ? 0.90 : 1.0
            Behavior on color { ColorAnimation { duration: 100 } }
            Behavior on scale { NumberAnimation { duration: 100; easing.type: Easing.OutQuad } }

            // Inner button dot detail
            Rectangle {
                width: 12
                height: 12
                radius: 6
                anchors.centerIn: parent
                color: isPressed ? "#101010" : "#666666"
                Behavior on color { ColorAnimation { duration: 100 } }
            }
        }
    }
}
