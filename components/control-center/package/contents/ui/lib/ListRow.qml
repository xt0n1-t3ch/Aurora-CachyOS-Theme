// SPDX-License-Identifier: GPL-3.0-only
// A row of a detail panel: a round glyph (blue when the item is in use), a name, and a note or a checkmark.
import QtQuick

Item {
    id: row

    property string glyph
    property string title
    // The text at the right end. A checkmark replaces it when checked is true, and an ellipsis while busy is true.
    property string note
    // The item is in use, so its round glyph turns blue.
    property bool on
    property bool checked
    property bool busy
    signal clicked()

    width: ListView.view ? ListView.view.width : parent.width
    height: 40
    Accessible.role: Accessible.Button
    Accessible.name: title

    Rectangle {
        anchors.fill: parent
        anchors.leftMargin: 6
        anchors.rightMargin: 6
        radius: 9
        color: Tokens.rowHover
        visible: area.containsMouse
    }

    Rectangle {
        id: badge
        x: 14
        anchors.verticalCenter: parent.verticalCenter
        width: 28
        height: 28
        radius: 14
        color: row.on ? Tokens.accent : Qt.rgba(1, 1, 1, 0.14)

        Symbol {
            anchors.centerIn: parent
            width: 16
            height: 16
            name: row.glyph
        }
    }

    Text {
        anchors.left: badge.right
        anchors.leftMargin: 10
        anchors.right: trailing.left
        anchors.rightMargin: 8
        anchors.verticalCenter: parent.verticalCenter
        text: row.title
        color: Tokens.text
        font.family: Tokens.font
        font.pixelSize: Tokens.titleSize
        elide: Text.ElideRight
    }

    Item {
        id: trailing
        anchors.right: parent.right
        anchors.rightMargin: 16
        anchors.verticalCenter: parent.verticalCenter
        width: row.checked ? 16 : noteText.implicitWidth
        height: 16

        Symbol {
            anchors.fill: parent
            visible: row.checked
            name: "checkmark"
            tone: "blue"
        }

        Text {
            id: noteText
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            visible: !row.checked
            text: row.busy ? "…" : row.note
            color: Tokens.secondaryText
            font.family: Tokens.font
            font.pixelSize: Tokens.sectionSize
        }
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: row.clicked()
    }
}
