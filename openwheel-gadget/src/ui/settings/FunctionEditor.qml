// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property var functionData: null
    signal functionChanged(var updated)
    signal deleteRequested()

    readonly property color bg:     "#0d0d0d"
    readonly property color bg2:    "#161616"
    readonly property color bg3:    "#1f1f1f"
    readonly property color bdr:    "#2a2a2a"
    readonly property color fg:     "#e8e8e8"
    readonly property color fg2:    "#888888"
    readonly property color accent: "#ffffff"
    readonly property string mono:  "monospace"

    height: expanded ? contentColumn.y + contentColumn.implicitHeight + 12 : 34
    Behavior on height { NumberAnimation { duration: 150; easing.type: Easing.OutQuad } }

    color: expanded ? bg2 : bg
    border.color: expanded ? bdr : "transparent"
    clip: true

    property bool expanded: false

    // ── Header ────────────────────────────────────────────────────────────────
    // Toggle MouseArea declared FIRST so it has lower Z than headerRow children.
    // This ensures the delete button inside headerRow receives clicks before
    // the toggle handler can intercept them.
    MouseArea {
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        height: 34
        onClicked: root.expanded = !root.expanded
    }

    RowLayout {
        id: headerRow
        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
        anchors.leftMargin: 10; anchors.rightMargin: 6
        height: 34
        spacing: 8

        Text {
            text: expanded ? "▾" : "▸"
            font.family: mono; font.pixelSize: 10; color: fg2
        }
        Text {
            text: functionData ? (functionData.label || functionData.id || "unnamed") : ""
            font.family: mono; font.pixelSize: 12; color: expanded ? fg : fg2
            Layout.fillWidth: true
        }
        Text {
            text: functionData ? (functionData.type === "discrete" ? "discrete" : "continuous") : ""
            font.family: mono; font.pixelSize: 10; color: fg2
        }
        // delete
        Rectangle {
            width: 20; height: 20; color: "transparent"; radius: 2
            Text { anchors.centerIn: parent; text: "×"; font.family: mono; font.pixelSize: 14; color: fg2 }
            MouseArea {
                anchors.fill: parent; hoverEnabled: true
                onEntered: parent.color = "#2a0000"
                onExited:  parent.color = "transparent"
                onClicked: root.deleteRequested()
            }
        }
    }

    // ── Expanded body ─────────────────────────────────────────────────────────
    Column {
        id: contentColumn
        anchors { left: parent.left; right: parent.right; top: headerRow.bottom }
        anchors.leftMargin: 10; anchors.rightMargin: 10; anchors.topMargin: 2
        spacing: 8
        visible: expanded

        Rectangle { width: parent.width; height: 1; color: bdr }

        // Properties grid
        GridLayout {
            width: parent.width
            columns: 4
            columnSpacing: 12; rowSpacing: 4

            Repeater {
                model: [
                    { lbl: "label", field: "label" },
                    { lbl: "unit",  field: "unit"  }
                ]
                Row {
                    spacing: 8
                    Text { text: modelData.lbl; font.family: mono; font.pixelSize: 10; color: fg2; width: 40; verticalAlignment: Text.AlignVCenter; height: 24 }
                    TextField {
                        width: 130; height: 24
                        font.family: mono; font.pixelSize: 11; color: fg
                        text: functionData ? (functionData[modelData.field] || "") : ""
                        background: Rectangle { color: bg3; border.color: bdr; radius: 2 }
                        Component.onCompleted: {
                            if (modelData.field === "label") labelField = this
                            else unitField = this
                        }
                        onTextEdited: emitUpdate()
                    }
                }
            }

            Row {
                spacing: 8
                Text { text: "type"; font.family: mono; font.pixelSize: 10; color: fg2; width: 40; verticalAlignment: Text.AlignVCenter; height: 24 }
                ComboBox {
                    id: typeBox
                    model: ["continuous", "discrete"]
                    currentIndex: functionData && functionData.type === "discrete" ? 1 : 0
                    width: 110; height: 24
                    font.family: mono; font.pixelSize: 11
                    contentItem: Text { leftPadding: 6; text: typeBox.displayText; font: typeBox.font; color: fg; verticalAlignment: Text.AlignVCenter }
                    background: Rectangle { color: bg3; border.color: bdr; radius: 2 }
                    popup: Popup {
                        y: typeBox.height; width: typeBox.width
                        padding: 0
                        background: Rectangle { color: bg3; border.color: bdr; radius: 2 }
                        contentItem: ListView {
                            implicitHeight: contentHeight
                            model: typeBox.delegateModel
                        }
                    }
                    delegate: ItemDelegate {
                        width: typeBox.width; height: 24
                        contentItem: Text { leftPadding: 6; text: modelData; font.family: mono; font.pixelSize: 11; color: fg; verticalAlignment: Text.AlignVCenter }
                        background: Rectangle { color: hovered ? bdr : bg3 }
                    }
                    onCurrentIndexChanged: emitUpdate()
                }
            }
        }

        // Action slots
        Text { text: "actions"; font.family: mono; font.pixelSize: 10; color: fg2 }

        ActionEditor { id: cwEditor;    width: parent.width; slotLabel: "↻"; actionData: functionData ? (functionData.clockwiseAction        || defAction()) : defAction(); onActionChanged: emitUpdate() }
        ActionEditor { id: ccwEditor;   width: parent.width; slotLabel: "↺"; actionData: functionData ? (functionData.counterClockwiseAction || defAction()) : defAction(); onActionChanged: emitUpdate() }
        ActionEditor { id: clickEditor; width: parent.width; slotLabel: "●"; actionData: functionData ? (functionData.clickAction           || defAction()) : defAction(); onActionChanged: emitUpdate() }

        Item { height: 2 }
    }

    // internal refs populated by Repeater above
    property var labelField: null
    property var unitField: null

    function defAction() {
        return { type: "keyboard", keys: "", modifiers: [], rotationThreshold: 15, accelerationEnabled: true, delta: 0 }
    }

    function emitUpdate() {
        if (!functionData) return
        var updated = {
            id:    functionData.id,
            label: labelField ? labelField.text : (functionData.label || ""),
            iconName: functionData.iconName || "",
            type:  typeBox.currentIndex === 1 ? "discrete" : "continuous",
            unit:  unitField ? unitField.text : (functionData.unit || ""),
            clockwiseAction:        cwEditor.actionData,
            counterClockwiseAction: ccwEditor.actionData,
            clickAction:            clickEditor.actionData
        }
        if (functionData.minValue !== undefined) updated.minValue = functionData.minValue
        if (functionData.maxValue !== undefined) updated.maxValue = functionData.maxValue
        root.functionChanged(updated)
    }
}
