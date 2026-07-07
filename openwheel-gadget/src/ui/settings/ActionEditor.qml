// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    property string slotLabel: "action"
    property var actionData: defAction()
    signal actionChanged(var updated)

    readonly property color bg3:   "#1f1f1f"
    readonly property color bdr:   "#2a2a2a"
    readonly property color fg:    "#e8e8e8"
    readonly property color fg2:   "#888888"
    readonly property string mono: "monospace"

    implicitHeight: col.implicitHeight + 10
    color: "transparent"

    property var _w: defAction()
    property bool _loading: false

    readonly property var typeNames:  ["keyboard", "keyboardRepeat", "mouseScroll", "dbus", "command"]
    readonly property var typeLabels: ["keyboard", "repeat", "scroll", "dbus", "command"]

    function typeIdx(name) {
        var i = typeNames.indexOf(name)
        return i >= 0 ? i : 0
    }

    function sync() {
        _loading = true
        _w = actionData ? JSON.parse(JSON.stringify(actionData)) : defAction()
        typeSelector.current = typeIdx(_w.type || "keyboard")
        keysField.text       = _w.keys || ""
        modCtrl.checked      = (_w.modifiers || []).indexOf("ctrl")  >= 0
        modShift.checked     = (_w.modifiers || []).indexOf("shift") >= 0
        modAlt.checked       = (_w.modifiers || []).indexOf("alt")   >= 0
        modSuper.checked     = (_w.modifiers || []).indexOf("meta")  >= 0
        threshSpin.value     = _w.rotationThreshold || 15
        deltaSpin.value      = _w.delta || 0
        svcField.text        = _w.dbusService   || ""
        pathField.text       = _w.dbusPath      || ""
        ifaceField.text      = _w.dbusInterface || ""
        methodField.text     = _w.dbusMethod    || ""
        cmdField.text        = _w.command       || ""
        _loading = false
    }

    onActionDataChanged: sync()
    Component.onCompleted: sync()

    function collect() {
        if (_loading) return
        // Capture fields with no UI control before overwriting _w.
        var prevSticky = _w.sticky || false
        var mods = []
        if (modCtrl.checked)  mods.push("ctrl")
        if (modShift.checked) mods.push("shift")
        if (modAlt.checked)   mods.push("alt")
        if (modSuper.checked) mods.push("meta")
        _w = {
            type: typeNames[typeSelector.current] || "keyboard",
            keys: keysField.text,
            modifiers: mods,
            rotationThreshold: threshSpin.value,
            accelerationEnabled: true,
            delta: deltaSpin.value,
            dbusService:   svcField.text,
            dbusPath:      pathField.text,
            dbusInterface: ifaceField.text,
            dbusMethod:    methodField.text,
            command:       cmdField.text
        }
        if (prevSticky) _w.sticky = true
        root.actionData = _w
        root.actionChanged(_w)
    }

    function defAction() {
        return { type: "keyboard", keys: "", modifiers: [],
                 rotationThreshold: 15, accelerationEnabled: true, delta: 0 }
    }

    // Internal type state
    QtObject {
        id: typeSelector
        property int current: 0
        onCurrentChanged: collect()
    }

    Column {
        id: col
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: 4
        spacing: 4

        // ── Row 1: label | type tabs | test ──────────────────────────────────
        Row {
            width: parent.width
            spacing: 6

            Text {
                text: slotLabel
                font.family: mono
                font.pixelSize: 11
                color: fg2
                width: 36
                height: 24
                verticalAlignment: Text.AlignVCenter
            }

            // Inline type tabs
            Row {
                spacing: 2
                Repeater {
                    model: typeLabels
                    Rectangle {
                        width: tabLabel.implicitWidth + 12
                        height: 24
                        color: index === typeSelector.current ? bg3 : "transparent"
                        border.color: index === typeSelector.current ? bdr : "transparent"
                        radius: 2
                        Text {
                            id: tabLabel
                            anchors.centerIn: parent
                            text: modelData
                            font.family: mono
                            font.pixelSize: 11
                            color: index === typeSelector.current ? fg : fg2
                        }
                        MouseArea {
                            anchors.fill: parent
                            hoverEnabled: true
                            onEntered: if (index !== typeSelector.current) parent.color = "#1a1a1a"
                            onExited: parent.color = index === typeSelector.current ? bg3 : "transparent"
                            onClicked: typeSelector.current = index
                        }
                    }
                }
            }

            Item { width: parent.width - 36 - typeRow.implicitWidth - testBtn.width - 18; height: 1 }

            Rectangle {
                id: testBtn
                width: 36
                height: 24
                color: "transparent"
                border.color: bdr
                radius: 2
                Text {
                    anchors.centerIn: parent
                    text: "test"
                    font.family: mono
                    font.pixelSize: 10
                    color: fg2
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: parent.border.color = fg
                    onExited: parent.border.color = bdr
                    onClicked: dialController.testAction(_w)
                }
            }
        }

        // ── Keyboard fields ───────────────────────────────────────────────────
        Column {
            width: parent.width
            spacing: 4
            visible: typeSelector.current === 0 || typeSelector.current === 1

            Row {
                spacing: 8
                Item { width: 36; height: 24 }
                TextField {
                    id: keysField
                    width: 160
                    height: 24
                    font.family: mono
                    font.pixelSize: 11
                    color: fg
                    placeholderText: "key name"
                    background: Rectangle { color: bg3; border.color: bdr; radius: 2 }
                    onTextEdited: collect()
                }
                // Modifier checkboxes
                Row {
                    spacing: 10
                    Repeater {
                        model: ["ctrl", "shift", "alt", "super"]
                        Row {
                            spacing: 3
                            Rectangle {
                                width: 14
                                height: 14
                                y: 5
                                color: modCheck.checked ? fg : bg3
                                border.color: modCheck.checked ? fg : bdr
                                radius: 2
                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: modCheck.checked = !modCheck.checked
                                }
                                CheckBox {
                                    id: modCheck
                                    visible: false
                                    checked: {
                                        var mods = root._w.modifiers || []
                                        if (index === 0) return mods.indexOf("ctrl")  >= 0
                                        if (index === 1) return mods.indexOf("shift") >= 0
                                        if (index === 2) return mods.indexOf("alt")   >= 0
                                        return mods.indexOf("meta") >= 0
                                    }
                                    onCheckedChanged: {
                                        if (!root._loading) {
                                            if (index === 0) modCtrl.checked  = checked
                                            if (index === 1) modShift.checked = checked
                                            if (index === 2) modAlt.checked   = checked
                                            if (index === 3) modSuper.checked = checked
                                            root.collect()
                                        }
                                    }
                                }
                            }
                            Text {
                                text: modelData
                                font.family: mono
                                font.pixelSize: 10
                                color: fg2
                                height: 24
                                verticalAlignment: Text.AlignVCenter
                            }
                        }
                    }
                }
            }

            Row {
                spacing: 8
                Item { width: 36; height: 24 }
                Text {
                    text: "threshold"
                    font.family: mono
                    font.pixelSize: 10
                    color: fg2
                    height: 24
                    verticalAlignment: Text.AlignVCenter
                }
                SpinBox {
                    id: threshSpin
                    from: 1; to: 360; value: 15
                    width: 90; height: 24
                    font.family: mono
                    font.pixelSize: 11
                    background: Rectangle { color: bg3; border.color: bdr; radius: 2 }
                    contentItem: TextInput {
                        text: threshSpin.textFromValue(threshSpin.value)
                        font: threshSpin.font
                        color: fg
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        readOnly: true
                    }
                    up.indicator: Rectangle {
                        x: threshSpin.width - width
                        width: 22; height: threshSpin.height
                        color: bg3
                        Text { anchors.centerIn: parent; text: "+"; font.family: mono; font.pixelSize: 13; color: fg2 }
                        MouseArea { anchors.fill: parent; onClicked: threshSpin.increase() }
                    }
                    down.indicator: Rectangle {
                        width: 22; height: threshSpin.height
                        color: bg3
                        Text { anchors.centerIn: parent; text: "−"; font.family: mono; font.pixelSize: 13; color: fg2 }
                        MouseArea { anchors.fill: parent; onClicked: threshSpin.decrease() }
                    }
                    onValueChanged: collect()
                }
                Text {
                    text: "°"
                    font.family: mono; font.pixelSize: 10; color: fg2
                    height: 24; verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // ── Scroll delta ──────────────────────────────────────────────────────
        Row {
            spacing: 8
            visible: typeSelector.current === 2
            Item { width: 36; height: 24 }
            Text {
                text: "delta"
                font.family: mono; font.pixelSize: 10; color: fg2
                height: 24; verticalAlignment: Text.AlignVCenter
            }
            SpinBox {
                id: deltaSpin
                from: -20; to: 20; value: 1
                width: 90; height: 24
                font.family: mono; font.pixelSize: 11
                background: Rectangle { color: bg3; border.color: bdr; radius: 2 }
                contentItem: TextInput {
                    text: deltaSpin.textFromValue(deltaSpin.value)
                    font: deltaSpin.font; color: fg
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    readOnly: true
                }
                up.indicator: Rectangle {
                    x: deltaSpin.width - width
                    width: 22; height: deltaSpin.height
                    color: bg3
                    Text { anchors.centerIn: parent; text: "+"; font.family: mono; font.pixelSize: 13; color: fg2 }
                    MouseArea { anchors.fill: parent; onClicked: deltaSpin.increase() }
                }
                down.indicator: Rectangle {
                    width: 22; height: deltaSpin.height
                    color: bg3
                    Text { anchors.centerIn: parent; text: "−"; font.family: mono; font.pixelSize: 13; color: fg2 }
                    MouseArea { anchors.fill: parent; onClicked: deltaSpin.decrease() }
                }
                onValueChanged: collect()
            }
        }

        // ── D-Bus fields ──────────────────────────────────────────────────────
        Column {
            width: parent.width
            spacing: 3
            visible: typeSelector.current === 3

            Repeater {
                model: ["service", "path", "interface", "method"]
                Row {
                    spacing: 8
                    Item { width: 36; height: 24 }
                    Text {
                        text: modelData
                        font.family: mono; font.pixelSize: 10; color: fg2
                        width: 56; height: 24; verticalAlignment: Text.AlignVCenter
                    }
                    TextField {
                        width: 260; height: 24
                        font.family: mono; font.pixelSize: 11; color: fg
                        background: Rectangle { color: bg3; border.color: bdr; radius: 2 }
                        text: {
                            if (index === 0) return root._w.dbusService   || ""
                            if (index === 1) return root._w.dbusPath      || ""
                            if (index === 2) return root._w.dbusInterface || ""
                            return root._w.dbusMethod || ""
                        }
                        onTextEdited: {
                            if (index === 0) svcField.text    = text
                            if (index === 1) pathField.text   = text
                            if (index === 2) ifaceField.text  = text
                            if (index === 3) methodField.text = text
                            root.collect()
                        }
                    }
                }
            }
        }

        // ── Command ───────────────────────────────────────────────────────────
        Row {
            spacing: 8
            visible: typeSelector.current === 4
            Item { width: 36; height: 24 }
            TextField {
                id: cmdField
                width: 320; height: 24
                font.family: mono; font.pixelSize: 11; color: fg
                placeholderText: "shell command"
                background: Rectangle { color: bg3; border.color: bdr; radius: 2 }
                onTextEdited: collect()
            }
        }
    }

    // State holders for modifier checkboxes (referenced by collect())
    CheckBox { id: modCtrl;  visible: false; onCheckedChanged: if (!root._loading) root.collect() }
    CheckBox { id: modShift; visible: false; onCheckedChanged: if (!root._loading) root.collect() }
    CheckBox { id: modAlt;   visible: false; onCheckedChanged: if (!root._loading) root.collect() }
    CheckBox { id: modSuper; visible: false; onCheckedChanged: if (!root._loading) root.collect() }

    // State holders for D-Bus fields
    TextField { id: svcField;    visible: false }
    TextField { id: pathField;   visible: false }
    TextField { id: ifaceField;  visible: false }
    TextField { id: methodField; visible: false }

    // Spacer reference for layout (unused but avoids binding warning)
    Item { id: typeRow; visible: false; implicitWidth: 200 }
}
