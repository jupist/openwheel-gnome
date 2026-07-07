// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 OpenWheel Contributors

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQuick.Dialogs

ApplicationWindow {
    id: settingsWindow
    title: "openwheel"
    width: 780
    height: 540
    minimumWidth: 600
    minimumHeight: 400
    color: "#0d0d0d"

    // ── Palette ──────────────────────────────────────────────────────────────
    readonly property color bg:      "#0d0d0d"
    readonly property color bg2:     "#161616"
    readonly property color bg3:     "#1f1f1f"
    readonly property color border:  "#2a2a2a"
    readonly property color fg:      "#e8e8e8"
    readonly property color fg2:     "#888888"
    readonly property color accent:  "#ffffff"
    readonly property string mono:   "monospace"

    // ── State ────────────────────────────────────────────────────────────────
    property string selectedProfileId: ""
    property var profileData: null
    property bool dirty: false
    property int expandedFunctionIndex: -1

    function selectProfile(id) {
        if (dirty) { discardDialog.pendingId = id; discardDialog.open(); return }
        doSelectProfile(id)
    }
    function doSelectProfile(id) {
        selectedProfileId = id
        profileData = JSON.parse(JSON.stringify(dialController.getProfileData(id)))
        dirty = false
        expandedFunctionIndex = -1
    }
    function saveCurrentProfile() {
        if (!profileData) return
        // Pass JSON string directly — avoids QVariantMap→QJsonObject conversion
        // issues with nested arrays when going through the QML→C++ bridge.
        var json = JSON.stringify(profileData)
        if (dialController.saveProfileDataJson(json) !== 0) {
            dirty = false; statusText.text = "saved."; statusClear.restart()
        } else {
            statusText.text = "save failed!"; statusClear.restart()
        }
    }
    function markDirty() { dirty = true }

    Timer { id: statusClear; interval: 3000; onTriggered: statusText.text = "" }

    // ── Help popup ────────────────────────────────────────────────────────────
    Popup {
        id: helpPopup
        modal: true; anchors.centerIn: Overlay.overlay
        width: 380; padding: 0
        background: Rectangle { color: bg2; border.color: border; radius: 3 }

        ColumnLayout {
            anchors.left: parent.left; anchors.right: parent.right
            spacing: 0

            // Title bar
            Rectangle {
                Layout.fillWidth: true; height: 36; color: bg3
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: border }
                RowLayout {
                    anchors.fill: parent; anchors.leftMargin: 14; anchors.rightMargin: 6
                    Text { text: "how to use profiles"; font.family: mono; font.pixelSize: 12; color: fg; Layout.fillWidth: true }
                    Rectangle {
                        width: 28; height: 28; color: "transparent"; radius: 2
                        Text { anchors.centerIn: parent; text: "×"; font.family: mono; font.pixelSize: 15; color: fg2 }
                        MouseArea { anchors.fill: parent; onClicked: helpPopup.close() }
                    }
                }
            }

            // Content
            ColumnLayout {
                Layout.margins: 16; spacing: 14

                Repeater {
                    model: [
                        {
                            heading: "creating a profile",
                            body:    "Click + in the sidebar to create a new profile. Give it a name — ideally matching the app it's for (e.g. \"Blender\"). Each profile is activated automatically when that app is focused, if you set a window class or process name in the JSON."
                        },
                        {
                            heading: "adding functions",
                            body:    "With a profile selected, click '+ add' under functions. Each function is one thing the dial does — e.g. 'Brush Size' or 'Timeline Scrub'. Give it a label, then set the clockwise (↻), counter-clockwise (↺), and click (●) actions."
                        },
                        {
                            heading: "action types",
                            body:    "keyboard — injects a key + optional modifiers (ctrl, shift, alt, super).\nrepeat — same but fires repeatedly while held.\nscroll — sends a mouse scroll event, use the delta field.\ndbus — calls a D-Bus method directly.\ncommand — runs a shell command."
                        },
                        {
                            heading: "threshold",
                            body:    "How many degrees the dial must turn before the action fires. Lower = more sensitive. 1° fires on every tick; 5° requires a deliberate turn. Scroll and window switching ignore this."
                        },
                        {
                            heading: "editing & saving",
                            body:    "Expand a function card to edit it. Changes are not applied until you click Save (or press Ctrl+S). The title bar shows * when there are unsaved changes. Closing without saving discards all edits."
                        },
                        {
                            heading: "deleting a profile",
                            body:    "Select the profile in the sidebar and click − to delete it. Built-in profiles (System, Blender, Krita…) cannot be deleted — only disabled via the dot on the right of each row."
                        }
                    ]

                    ColumnLayout {
                        Layout.fillWidth: true; spacing: 3
                        Text {
                            text: modelData.heading
                            font.family: mono; font.pixelSize: 11; font.weight: Font.Bold
                            color: fg
                        }
                        Text {
                            Layout.fillWidth: true
                            text: modelData.body
                            font.family: mono; font.pixelSize: 10; color: fg2
                            wrapMode: Text.WordWrap
                            lineHeight: 1.35
                        }
                    }
                }

                // Close button
                Rectangle {
                    Layout.alignment: Qt.AlignRight
                    height: 24; width: closeHelpLabel.implicitWidth + 20
                    color: "transparent"; border.color: border; radius: 2
                    Text { id: closeHelpLabel; anchors.centerIn: parent; text: "close"; font.family: mono; font.pixelSize: 11; color: fg2 }
                    MouseArea {
                        anchors.fill: parent; hoverEnabled: true
                        onEntered: parent.border.color = fg2
                        onExited:  parent.border.color = border
                        onClicked: helpPopup.close()
                    }
                }
            }
        }
    }

    // Use allProfiles() so disabled profiles are also shown in the settings UI.
    function realProfiles() {
        return dialController.allProfiles()
    }

    Connections {
        target: dialController
        function onProfilesChanged() { profileListView.model = settingsWindow.realProfiles() }
    }

    // ── Dialogs (custom dark popups — avoids native button box theming) ────────

    Popup {
        id: discardDialog
        property string pendingId: ""
        modal: true; anchors.centerIn: Overlay.overlay
        width: 300; padding: 0
        background: Rectangle { color: bg2; border.color: border; radius: 3 }
        ColumnLayout {
            anchors.left: parent.left; anchors.right: parent.right
            spacing: 0
            Rectangle {
                Layout.fillWidth: true; height: 36; color: bg3
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: border }
                Text { anchors.centerIn: parent; text: "discard changes?"; font.family: mono; font.pixelSize: 12; color: fg }
            }
            Text {
                Layout.leftMargin: 16; Layout.rightMargin: 16; Layout.topMargin: 14; Layout.bottomMargin: 14
                text: "unsaved changes will be lost."
                font.family: mono; font.pixelSize: 11; color: fg2
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: border }
            RowLayout {
                Layout.fillWidth: true; Layout.margins: 8; spacing: 6
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 64; height: 26; radius: 2; color: "transparent"; border.color: border
                    Text { anchors.centerIn: parent; text: "cancel"; font.family: mono; font.pixelSize: 11; color: fg2 }
                    MouseArea { anchors.fill: parent; onClicked: discardDialog.close() }
                }
                Rectangle {
                    width: 72; height: 26; radius: 2; color: "#2a0000"; border.color: "#553333"
                    Text { anchors.centerIn: parent; text: "discard"; font.family: mono; font.pixelSize: 11; color: "#cc5555" }
                    MouseArea { anchors.fill: parent; onClicked: { discardDialog.close(); settingsWindow.doSelectProfile(discardDialog.pendingId) } }
                }
            }
        }
    }

    Popup {
        id: newProfileDialog
        modal: true; anchors.centerIn: Overlay.overlay
        width: 300; padding: 0
        background: Rectangle { color: bg2; border.color: border; radius: 3 }
        onOpened: { newProfileName.text = ""; newProfileName.forceActiveFocus() }
        ColumnLayout {
            anchors.left: parent.left; anchors.right: parent.right
            spacing: 0
            Rectangle {
                Layout.fillWidth: true; height: 36; color: bg3
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: border }
                Text { anchors.centerIn: parent; text: "new profile"; font.family: mono; font.pixelSize: 12; color: fg }
            }
            ColumnLayout {
                Layout.leftMargin: 16; Layout.rightMargin: 16; Layout.topMargin: 14; Layout.bottomMargin: 14
                spacing: 6
                Text { text: "name"; font.family: mono; font.pixelSize: 11; color: fg2 }
                TextField {
                    id: newProfileName
                    Layout.fillWidth: true; height: 28
                    font.family: mono; font.pixelSize: 12; color: fg
                    placeholderText: "my app"
                    background: Rectangle { color: bg3; border.color: border; radius: 2 }
                    Keys.onReturnPressed: newProfileDialog._create()
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: border }
            RowLayout {
                Layout.fillWidth: true; Layout.margins: 8; spacing: 6
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 64; height: 26; radius: 2; color: "transparent"; border.color: border
                    Text { anchors.centerIn: parent; text: "cancel"; font.family: mono; font.pixelSize: 11; color: fg2 }
                    MouseArea { anchors.fill: parent; onClicked: newProfileDialog.close() }
                }
                Rectangle {
                    width: 64; height: 26; radius: 2; color: accent
                    Text { anchors.centerIn: parent; text: "create"; font.family: mono; font.pixelSize: 11; color: bg }
                    MouseArea { anchors.fill: parent; onClicked: newProfileDialog._create() }
                }
            }
        }
        function _create() {
            var name = newProfileName.text.trim()
            if (!name) return
            close()
            var id = dialController.createNewProfile(name)
            if (id) settingsWindow.doSelectProfile(id)
        }
    }

    Popup {
        id: deleteProfileDialog
        modal: true; anchors.centerIn: Overlay.overlay
        width: 300; padding: 0
        background: Rectangle { color: bg2; border.color: border; radius: 3 }
        ColumnLayout {
            anchors.left: parent.left; anchors.right: parent.right
            spacing: 0
            Rectangle {
                Layout.fillWidth: true; height: 36; color: bg3
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: border }
                Text { anchors.centerIn: parent; text: "delete profile?"; font.family: mono; font.pixelSize: 12; color: fg }
            }
            Text {
                Layout.leftMargin: 16; Layout.rightMargin: 16; Layout.topMargin: 14; Layout.bottomMargin: 14
                Layout.fillWidth: true
                text: profileData ? '"' + (profileData.displayName || profileData.id) + '" will be permanently deleted.' : ""
                font.family: mono; font.pixelSize: 11; color: fg2
                wrapMode: Text.Wrap
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: border }
            RowLayout {
                Layout.fillWidth: true; Layout.margins: 8; spacing: 6
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 64; height: 26; radius: 2; color: "transparent"; border.color: border
                    Text { anchors.centerIn: parent; text: "cancel"; font.family: mono; font.pixelSize: 11; color: fg2 }
                    MouseArea { anchors.fill: parent; onClicked: deleteProfileDialog.close() }
                }
                Rectangle {
                    width: 64; height: 26; radius: 2; color: "#2a0000"; border.color: "#553333"
                    Text { anchors.centerIn: parent; text: "delete"; font.family: mono; font.pixelSize: 11; color: "#cc5555" }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            deleteProfileDialog.close()
                            if (settingsWindow.selectedProfileId && dialController.deleteProfile(settingsWindow.selectedProfileId) !== 0) {
                                settingsWindow.selectedProfileId = ""; settingsWindow.profileData = null; settingsWindow.dirty = false
                            }
                        }
                    }
                }
            }
        }
    }

    Popup {
        id: newFunctionDialog
        modal: true; anchors.centerIn: Overlay.overlay
        width: 300; padding: 0
        background: Rectangle { color: bg2; border.color: border; radius: 3 }
        onOpened: { newFuncLabel.text = ""; newFuncLabel.forceActiveFocus() }
        ColumnLayout {
            anchors.left: parent.left; anchors.right: parent.right
            spacing: 0
            Rectangle {
                Layout.fillWidth: true; height: 36; color: bg3
                Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: border }
                Text { anchors.centerIn: parent; text: "add function"; font.family: mono; font.pixelSize: 12; color: fg }
            }
            ColumnLayout {
                Layout.leftMargin: 16; Layout.rightMargin: 16; Layout.topMargin: 14; Layout.bottomMargin: 14
                spacing: 6
                Text { text: "label"; font.family: mono; font.pixelSize: 11; color: fg2 }
                TextField {
                    id: newFuncLabel
                    Layout.fillWidth: true; height: 28
                    font.family: mono; font.pixelSize: 12; color: fg
                    placeholderText: "brush size"
                    background: Rectangle { color: bg3; border.color: border; radius: 2 }
                    Keys.onReturnPressed: newFunctionDialog._add()
                }
            }
            Rectangle { Layout.fillWidth: true; height: 1; color: border }
            RowLayout {
                Layout.fillWidth: true; Layout.margins: 8; spacing: 6
                Item { Layout.fillWidth: true }
                Rectangle {
                    width: 64; height: 26; radius: 2; color: "transparent"; border.color: border
                    Text { anchors.centerIn: parent; text: "cancel"; font.family: mono; font.pixelSize: 11; color: fg2 }
                    MouseArea { anchors.fill: parent; onClicked: newFunctionDialog.close() }
                }
                Rectangle {
                    width: 64; height: 26; radius: 2; color: accent
                    Text { anchors.centerIn: parent; text: "add"; font.family: mono; font.pixelSize: 11; color: bg }
                    MouseArea { anchors.fill: parent; onClicked: newFunctionDialog._add() }
                }
            }
        }
        function _add() {
            var lbl = newFuncLabel.text.trim()
            if (!lbl || !settingsWindow.profileData) return
            close()
            var newId = lbl.toLowerCase().replace(/[^a-z0-9]+/g, "-").replace(/^-|-$/g, "") || "function"
            var existing = (settingsWindow.profileData.functions || []).map(function(f) { return f.id })
            var base = newId, i = 2
            while (existing.indexOf(newId) >= 0) { newId = base + "-" + i++ }
            var def = { type: "keyboard", keys: "", modifiers: [], rotationThreshold: 15, accelerationEnabled: true, delta: 0 }
            var f = { id: newId, label: lbl, iconName: "", type: "continuous", unit: "",
                      clockwiseAction: JSON.parse(JSON.stringify(def)),
                      counterClockwiseAction: JSON.parse(JSON.stringify(def)),
                      clickAction: JSON.parse(JSON.stringify(def)) }
            var funcs = (settingsWindow.profileData.functions || []).slice()
            funcs.push(f)
            settingsWindow.profileData.functions = funcs
            settingsWindow.profileData.defaultMenuLayout = (settingsWindow.profileData.defaultMenuLayout || []).concat([newId])
            settingsWindow.profileData = JSON.parse(JSON.stringify(settingsWindow.profileData))
            settingsWindow.markDirty()
        }
    }

    // ── Root layout ──────────────────────────────────────────────────────────
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header bar
        Rectangle {
            Layout.fillWidth: true
            height: 36
            color: bg2
            Rectangle {
                anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                height: 1; color: border
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 8
                spacing: 0

                Text {
                    text: "openwheel / settings" + (dirty ? " *" : "")
                    font.family: mono; font.pixelSize: 12
                    color: fg2
                    Layout.fillWidth: true
                }
                // help
                Rectangle {
                    width: 36; height: 36; color: "transparent"
                    Text { anchors.centerIn: parent; text: "?"; font.family: mono; font.pixelSize: 14; color: fg2 }
                    MouseArea {
                        anchors.fill: parent; hoverEnabled: true
                        onEntered: parent.color = bg3
                        onExited:  parent.color = "transparent"
                        onClicked: helpPopup.open()
                    }
                }
                // close
                Rectangle {
                    width: 36; height: 36; color: "transparent"
                    Text { anchors.centerIn: parent; text: "×"; font.family: mono; font.pixelSize: 16; color: fg2 }
                    MouseArea { anchors.fill: parent; onClicked: settingsWindow.close()
                        hoverEnabled: true
                        onEntered: parent.color = bg3
                        onExited: parent.color = "transparent"
                    }
                }
            }
        }

        // Main split
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            // ── Profile sidebar ───────────────────────────────────────────────
            Rectangle {
                Layout.preferredWidth: 180
                Layout.fillHeight: true
                color: bg2
                Rectangle {
                    anchors.top: parent.top; anchors.bottom: parent.bottom; anchors.right: parent.right
                    width: 1; color: border
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // ── Behaviour section ──────────────────────────────────
                    Text {
                        text: "behaviour"
                        font.family: mono; font.pixelSize: 10; color: fg2
                        leftPadding: 12; topPadding: 12; bottomPadding: 6
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 12; Layout.rightMargin: 10; Layout.bottomMargin: 8
                        spacing: 8
                        Text {
                            Layout.fillWidth: true
                            text: "press to activate"
                            font.family: mono; font.pixelSize: 11; color: fg
                            wrapMode: Text.WordWrap
                        }
                        Rectangle {
                            width: 32; height: 18; radius: 9
                            color: dialController.pressToActivate ? accent : "#444"
                            Behavior on color { ColorAnimation { duration: 120 } }
                            Rectangle {
                                width: 14; height: 14; radius: 7
                                x: dialController.pressToActivate ? parent.width - width - 2 : 2
                                anchors.verticalCenter: parent.verticalCenter
                                color: dialController.pressToActivate ? bg : "#aaa"
                                Behavior on x { NumberAnimation { duration: 120 } }
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: dialController.pressToActivate = !dialController.pressToActivate
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.leftMargin: 12; Layout.rightMargin: 10; Layout.bottomMargin: 8
                        spacing: 8
                        Text {
                            Layout.fillWidth: true
                            text: "start daemon on login"
                            font.family: mono; font.pixelSize: 11; color: fg
                            wrapMode: Text.WordWrap
                        }
                        Rectangle {
                            width: 32; height: 18; radius: 9
                            color: dialController.daemonAutostart ? accent : "#444"
                            Behavior on color { ColorAnimation { duration: 120 } }
                            Rectangle {
                                width: 14; height: 14; radius: 7
                                x: dialController.daemonAutostart ? parent.width - width - 2 : 2
                                anchors.verticalCenter: parent.verticalCenter
                                color: dialController.daemonAutostart ? bg : "#aaa"
                                Behavior on x { NumberAnimation { duration: 120 } }
                            }
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: dialController.daemonAutostart = dialController.daemonAutostart ? 0 : 1
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; height: 1; color: border }

                    // ── Profiles section ───────────────────────────────────
                    Text {
                        text: "profiles"
                        font.family: mono; font.pixelSize: 10
                        color: fg2
                        leftPadding: 12; topPadding: 10; bottomPadding: 8
                    }

                    ListView {
                        id: profileListView
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: settingsWindow.realProfiles()
                        clip: true
                        currentIndex: {
                            for (var i = 0; i < model.length; ++i)
                                if (model[i].id === selectedProfileId) return i
                            return -1
                        }

                        delegate: Rectangle {
                            width: profileListView.width
                            height: 32
                            color: modelData.id === selectedProfileId ? bg3 : "transparent"

                            Rectangle {
                                visible: modelData.id === selectedProfileId
                                anchors { left: parent.left; top: parent.top; bottom: parent.bottom }
                                width: 2; color: accent
                            }

                            Text {
                                anchors { left: parent.left; right: enableDot.left; verticalCenter: parent.verticalCenter }
                                anchors.leftMargin: 14
                                anchors.rightMargin: 6
                                text: modelData.name || modelData.id
                                font.family: mono; font.pixelSize: 12
                                color: modelData.id === selectedProfileId ? accent : fg2
                                elide: Text.ElideRight
                            }

                            // Row hover/select — defined first so enable dot (below) sits on top
                            MouseArea {
                                anchors.fill: parent
                                hoverEnabled: true
                                onEntered: if (modelData.id !== selectedProfileId) parent.color = "#1a1a1a"
                                onExited: parent.color = modelData.id === selectedProfileId ? bg3 : "transparent"
                                onClicked: selectProfile(modelData.id)
                            }

                            // Enable toggle — defined last so its MouseArea is on top of the row MA
                            Rectangle {
                                id: enableDot
                                anchors.right: parent.right
                                anchors.rightMargin: 10
                                anchors.verticalCenter: parent.verticalCenter
                                width: 8; height: 8; radius: 4
                                color: modelData.enabled ? accent : "#444"

                                MouseArea {
                                    anchors.fill: parent
                                    anchors.margins: -8   // generous 24×24 hit target around 8px dot
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        mouse.accepted = true
                                        dialController.setProfileEnabled(modelData.id, modelData.enabled ? 0 : 1)
                                        profileListView.model = settingsWindow.realProfiles()
                                    }
                                }
                            }
                        }
                    }

                    // Sidebar action row
                    Rectangle { Layout.fillWidth: true; height: 1; color: border }
                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: 6
                        spacing: 2

                        Repeater {
                            model: [
                                { label: "+",  tip: "new",    fn: function() { newProfileDialog.open() } },
                                { label: "−",  tip: "delete", fn: function() { deleteProfileDialog.open() } },
                                { label: "⟳",  tip: "reload", fn: function() { dialController.reloadAllProfiles(); statusText.text = "reloaded." } },
                                { label: "↑",  tip: "import", fn: function() { importFileDialog.open() } },
                                { label: "↓",  tip: "export", fn: function() { exportFileDialog.open() } }
                            ]
                            Rectangle {
                                width: 28; height: 24
                                color: "transparent"; radius: 2
                                // delete + export require a profile to be selected
                                enabled: (index === 1 || index === 4)
                                         ? (selectedProfileId.length > 0 && profileData && !profileData.isDefault)
                                         : true
                                opacity: enabled ? 1.0 : 0.3
                                Text { anchors.centerIn: parent; text: modelData.label; font.family: mono; font.pixelSize: 14; color: fg2 }
                                MouseArea {
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    onEntered: parent.color = bg3
                                    onExited:  parent.color = "transparent"
                                    onClicked: modelData.fn()
                                    enabled: parent.enabled
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                    }

                    // ── Import file dialog ─────────────────────────────────
                    FileDialog {
                        id: importFileDialog
                        title: "Import profile"
                        fileMode: FileDialog.OpenFile
                        nameFilters: ["OpenWheel profiles (*.json)", "All files (*)"]
                        onAccepted: {
                            var id = dialController.importProfile(selectedFile)
                            if (id) {
                                statusText.text = "imported: " + id
                                statusClear.restart()
                                settingsWindow.doSelectProfile(id)
                            } else {
                                statusText.text = "import failed!"
                                statusClear.restart()
                            }
                        }
                    }

                    // ── Export file dialog ─────────────────────────────────
                    FileDialog {
                        id: exportFileDialog
                        title: "Export profile"
                        fileMode: FileDialog.SaveFile
                        nameFilters: ["OpenWheel profiles (*.json)", "All files (*)"]
                        defaultSuffix: "json"
                        currentFile: selectedProfileId ? ("file://" + StandardPaths.writableLocation(StandardPaths.HomeLocation) + "/" + selectedProfileId + ".json") : ""
                        onAccepted: {
                            if (dialController.exportProfile(selectedProfileId, selectedFile) !== 0) {
                                statusText.text = "exported."
                            } else {
                                statusText.text = "export failed!"
                            }
                            statusClear.restart()
                        }
                    }
                }
            }

            // ── Right panel ───────────────────────────────────────────────────
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: bg

                // Placeholder
                Text {
                    anchors.centerIn: parent
                    visible: !profileData
                    text: "← select a profile"
                    font.family: mono; font.pixelSize: 12; color: fg2
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12
                    visible: profileData !== null

                    // Profile name row
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        Text { text: "name"; font.family: mono; font.pixelSize: 11; color: fg2 }

                        TextField {
                            id: profileNameField
                            text: profileData ? (profileData.displayName || "") : ""
                            font.family: mono; font.pixelSize: 12
                            color: fg
                            background: Rectangle { color: bg3; border.color: border; radius: 2 }
                            Layout.preferredWidth: 180
                            onTextEdited: { if (profileData) { profileData.displayName = text; markDirty() } }
                        }

                        Item { Layout.fillWidth: true }
                    }

                    // Functions header
                    RowLayout {
                        Layout.fillWidth: true
                        Text { text: "functions"; font.family: mono; font.pixelSize: 10; color: fg2 }
                        Rectangle { Layout.fillWidth: true; height: 1; color: border }
                        Rectangle {
                            height: 22; width: addText.implicitWidth + 16
                            color: "transparent"; border.color: border; radius: 2
                            Text {
                                id: addText
                                anchors.centerIn: parent
                                text: "+ add"
                                font.family: mono; font.pixelSize: 11; color: fg2
                            }
                            MouseArea {
                                anchors.fill: parent; hoverEnabled: true
                                onEntered: parent.border.color = fg2
                                onExited:  parent.border.color = border
                                onClicked: newFunctionDialog.open()
                            }
                        }
                    }

                    // Function list
                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                        Column {
                            width: parent.width
                            spacing: 1

                            Repeater {
                                model: profileData ? (profileData.functions || []) : []
                                FunctionEditor {
                                    width: parent.width
                                    functionData: modelData
                                    // Restore expanded state after model rebuilds
                                    Component.onCompleted: {
                                        if (index === settingsWindow.expandedFunctionIndex)
                                            expanded = true
                                    }
                                    onExpandedChanged: {
                                        if (expanded)
                                            settingsWindow.expandedFunctionIndex = index
                                        else if (settingsWindow.expandedFunctionIndex === index)
                                            settingsWindow.expandedFunctionIndex = -1
                                    }
                                    onFunctionChanged: function(updated) {
                                        if (!profileData) return
                                        var funcs = profileData.functions.slice()
                                        funcs[index] = updated
                                        profileData.functions = funcs
                                        profileData = JSON.parse(JSON.stringify(profileData))
                                        markDirty()
                                    }
                                    onDeleteRequested: {
                                        if (!profileData) return
                                        profileData.functions = profileData.functions.filter(function(f, i) { return i !== index })
                                        profileData.defaultMenuLayout = (profileData.defaultMenuLayout || []).filter(function(id) { return id !== modelData.id })
                                        profileData = JSON.parse(JSON.stringify(profileData))
                                        markDirty()
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // Status / action bar
        Rectangle {
            Layout.fillWidth: true
            height: 32
            color: bg2
            Rectangle {
                anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                height: 1; color: border
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 10
                spacing: 10

                Text {
                    id: statusText
                    text: ""
                    font.family: mono; font.pixelSize: 11; color: fg2
                    Layout.fillWidth: true
                }

                // Save button
                Rectangle {
                    height: 22; width: saveLabel.implicitWidth + 24
                    color: dirty ? accent : "transparent"
                    border.color: dirty ? accent : border
                    radius: 2
                    opacity: dirty ? 1.0 : 0.4
                    Text {
                        id: saveLabel
                        anchors.centerIn: parent
                        text: "save  ctrl+s"
                        font.family: mono; font.pixelSize: 11
                        color: dirty ? bg : fg2
                    }
                    MouseArea {
                        anchors.fill: parent
                        enabled: dirty
                        onClicked: saveCurrentProfile()
                    }
                }

                // Close button
                Rectangle {
                    height: 22; width: closeLabel.implicitWidth + 16
                    color: "transparent"; border.color: border; radius: 2
                    Text {
                        id: closeLabel
                        anchors.centerIn: parent
                        text: "close"
                        font.family: mono; font.pixelSize: 11; color: fg2
                    }
                    MouseArea {
                        anchors.fill: parent; hoverEnabled: true
                        onEntered: parent.border.color = fg2
                        onExited:  parent.border.color = border
                        onClicked: settingsWindow.close()
                    }
                }
            }
        }
    }

    Shortcut { sequence: "Ctrl+S"; onActivated: if (dirty) saveCurrentProfile() }
    Shortcut { sequence: "Escape"; onActivated: settingsWindow.close() }

    // Discard unsaved changes when the window is closed so that reopening
    // always shows the last saved state, not whatever was edited last session.
    onClosing: {
        if (dirty && selectedProfileId) doSelectProfile(selectedProfileId)
    }
}
