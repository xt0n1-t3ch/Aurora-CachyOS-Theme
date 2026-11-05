// SPDX-License-Identifier: GPL-3.0-only
// The Control Center itself: macOS Tahoe's grid of floating glass modules, the panel a module opens in its place,
// and Edit Controls below. Every module and panel is its own Glass; GlassBlur in the window frosts them.
import QtQuick
import org.kde.plasma.plasmoid
import org.kde.plasma.plasma5support as P5Support
import "lib"
import "modules"
import "panels"
import "js/cc.js" as CC

Item {
    id: cc

    // The detail panel that stands in for the grid ("network", "bluetooth" or "sound"); empty shows the grid.
    property string panel: ""
    // Emitted when the window should close, for example after a module opens another app.
    signal closeRequested()

    // Ids of the modules that are not shown: the ones switched off in Edit Controls, and Display when no monitor can
    // be dimmed.
    readonly property var hidden: {
        const list = []
        if (!Plasmoid.configuration.showMediaPlayer) list.push("media")
        if (!Plasmoid.configuration.showNightColor) list.push("nightlight")
        if (!Plasmoid.configuration.showDnd) list.push("focus")
        if (!Plasmoid.configuration.showVolume) list.push("sound")
        if (!Plasmoid.configuration.showBrightness || !display.available) list.push("display")
        return list
    }
    // Where each shown module sits in the grid.
    readonly property var placements: CC.layout(hidden)

    function openPanel(name) {
        panel = name
    }

    function close() {
        closeRequested()
    }

    // States other parts of the system can change while the Control Center is closed.
    function refresh() {
        nightLight.refresh()
    }

    // Runs a command line through Plasma's executable data engine and ignores what it prints.
    function run(command) {
        executable.connectSource(command)
    }

    implicitWidth: CC.gridWidth()
    // The grid, a 12 px gap and the 26 px Edit Controls button below it.
    implicitHeight: CC.heightOf(placements) + 12 + 26

    P5Support.DataSource {
        id: executable
        engine: "executable"
        onNewData: sourceName => disconnectSource(sourceName)
    }

    // The cell of one module: placed where cc.js puts the module called name, and hidden when it is not shown.
    component Slot: Item {
        property string name
        readonly property var place: CC.placementOf(cc.placements, name)
        visible: !!place
        x: place ? place.x : 0
        y: place ? place.y : 0
        width: place ? place.width : 0
        height: place ? place.height : 0
    }

    Item {
        id: grid
        width: CC.gridWidth()
        height: CC.heightOf(cc.placements)
        opacity: cc.panel === "" ? 1 : 0
        scale: cc.panel === "" ? 1 : 0.94
        visible: opacity > 0.01
        transformOrigin: Item.Top

        Behavior on opacity {
            NumberAnimation { duration: Tokens.medium; easing.type: Easing.OutCubic }
        }
        Behavior on scale {
            NumberAnimation { duration: Tokens.medium; easing.type: Easing.OutCubic }
        }

        // The gaps between modules belong to the Control Center: a click there does nothing.
        MouseArea {
            anchors.fill: parent
        }

        Slot {
            name: "network"
            NetworkModule { anchors.fill: parent; controlCenter: cc }
        }
        Slot {
            name: "bluetooth"
            BluetoothModule { anchors.fill: parent; controlCenter: cc }
        }
        Slot {
            name: "nightlight"
            NightLightModule { id: nightLight; anchors.fill: parent; controlCenter: cc }
        }
        Slot {
            name: "media"
            MediaModule { anchors.fill: parent; controlCenter: cc }
        }
        Slot {
            name: "overview"
            RoundButton {
                anchors.fill: parent
                glyph: "overview"
                label: "Overview"
                onClicked: {
                    cc.close()
                    // KWin's Overview effect, through its global shortcut.
                    cc.run("qdbus6 org.kde.kglobalaccel /component/kwin org.kde.kglobalaccel.Component.invokeShortcut Overview")
                }
            }
        }
        Slot {
            name: "mirroring"
            RoundButton {
                anchors.fill: parent
                glyph: "mirroring"
                label: "Display Modes"
                onClicked: {
                    cc.close()
                    // KScreen's display mode chooser.
                    cc.run("qdbus6 org.kde.kscreen.osdService /org/kde/kscreen/osdService org.kde.kscreen.osdService.showActionSelector")
                }
            }
        }
        Slot {
            name: "settings"
            RoundButton {
                anchors.fill: parent
                glyph: "gear"
                label: "System Settings"
                onClicked: {
                    cc.close()
                    cc.run("systemsettings")
                }
            }
        }
        Slot {
            name: "screenshot"
            RoundButton {
                anchors.fill: parent
                glyph: "screenshot"
                label: "Screenshot"
                onClicked: {
                    cc.close()
                    // Leave time for the Control Center to fade out so it is not in the picture.
                    shot.restart()
                }
                Timer {
                    id: shot
                    interval: 350
                    onTriggered: cc.run("spectacle -r")
                }
            }
        }
        Slot {
            name: "focus"
            FocusModule { anchors.fill: parent; controlCenter: cc }
        }
        Slot {
            name: "display"
            DisplayModule { id: display; anchors.fill: parent; controlCenter: cc }
        }
        Slot {
            name: "sound"
            SoundModule { anchors.fill: parent; controlCenter: cc }
        }
    }

    Loader {
        id: detail
        width: CC.gridWidth()
        active: cc.panel !== ""
        sourceComponent: cc.panel === "network" ? networkPanel : cc.panel === "bluetooth" ? bluetoothPanel : soundPanel
        onLoaded: entrance.restart()

        ParallelAnimation {
            id: entrance
            NumberAnimation { target: detail.item; property: "opacity"; from: 0; to: 1; duration: Tokens.medium; easing.type: Easing.OutCubic }
            NumberAnimation { target: detail.item; property: "scale"; from: 0.96; to: 1; duration: Tokens.medium; easing.type: Easing.OutCubic }
        }

        // Keeps a click on the panel from reaching the window's click area, which would go back to the grid.
        MouseArea {
            anchors.fill: parent
            z: -1
        }
    }

    Component {
        id: networkPanel
        NetworkPanel { controlCenter: cc; maximumHeight: cc.implicitHeight }
    }
    Component {
        id: bluetoothPanel
        BluetoothPanel { controlCenter: cc; maximumHeight: cc.implicitHeight }
    }
    Component {
        id: soundPanel
        SoundPanel { controlCenter: cc; maximumHeight: cc.implicitHeight }
    }

    Glass {
        id: edit
        anchors.horizontalCenter: parent.horizontalCenter
        y: grid.height + 12
        width: editLabel.implicitWidth + 28
        height: 26
        opacity: cc.panel === "" ? 1 : 0
        visible: opacity > 0.01
        lift: editArea.pressed ? 1 : editArea.containsMouse ? 0.5 : 0

        Behavior on opacity {
            NumberAnimation { duration: Tokens.medium }
        }

        Text {
            id: editLabel
            anchors.centerIn: parent
            text: "Edit Controls"
            color: Tokens.text
            font.family: Tokens.font
            font.pixelSize: Tokens.titleSize
        }

        MouseArea {
            id: editArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                cc.close()
                Plasmoid.internalAction("configure").trigger()
            }
        }
    }

}
