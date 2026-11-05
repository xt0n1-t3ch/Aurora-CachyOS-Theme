// SPDX-License-Identifier: GPL-3.0-only
// The panel a module opens in place of the grid (Wi-Fi, Bluetooth, Sound output on macOS): a title with an
// optional switch, a list, and a settings link at the bottom.
import QtQuick

Glass {
    id: panel

    property string title
    // The header switch (Wi-Fi, Bluetooth) shows when hasSwitch is true. It never flips itself: switchOn follows the
    // state of the thing it controls.
    property bool hasSwitch: false
    property bool switchOn: false
    // The small heading above the list.
    property string section
    property alias model: list.model
    property alias delegate: list.delegate
    // Shown in the middle of the panel while the list is empty.
    property string emptyText
    // The label of the link at the bottom.
    property string settingsText
    signal switchToggled()
    signal settingsClicked()

    radius: 26
    height: Math.min(maximumHeight, 48 + sectionLabel.height + Math.max(list.contentHeight, list.count === 0 ? 44 : 0) + 12 + 44)
    // The panel grows with its list up to this height, in px.
    property real maximumHeight: 400

    Text {
        x: 16
        y: 15
        text: panel.title
        color: Tokens.text
        font.family: Tokens.font
        font.pixelSize: Tokens.titleSize + 1
        font.weight: Font.DemiBold
    }

    Switch {
        visible: panel.hasSwitch
        anchors.right: parent.right
        anchors.rightMargin: 14
        y: 12
        checked: panel.switchOn
        onToggled: panel.switchToggled()
    }

    Text {
        id: sectionLabel
        x: 16
        y: 48
        visible: text.length > 0
        height: visible ? implicitHeight + 6 : 0
        text: panel.section
        color: Tokens.sectionText
        font.family: Tokens.font
        font.pixelSize: Tokens.sectionSize
        font.weight: Font.DemiBold
    }

    ListView {
        id: list
        anchors.top: sectionLabel.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: separator.top
        anchors.bottomMargin: 6
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height

        Text {
            anchors.centerIn: parent
            visible: list.count === 0 && panel.emptyText.length > 0
            text: panel.emptyText
            color: Tokens.secondaryText
            font.family: Tokens.font
            font.pixelSize: Tokens.subtitleSize
        }
    }

    Rectangle {
        id: separator
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.bottom: settings.top
        height: 1
        color: Tokens.separator
    }

    Item {
        id: settings
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 44

        Rectangle {
            anchors.fill: parent
            anchors.margins: 6
            radius: 9
            color: Tokens.rowHover
            visible: settingsArea.containsMouse
        }

        Text {
            x: 16
            anchors.verticalCenter: parent.verticalCenter
            text: panel.settingsText
            color: Tokens.text
            font.family: Tokens.font
            font.pixelSize: Tokens.titleSize
        }

        MouseArea {
            id: settingsArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: panel.settingsClicked()
        }
    }
}
