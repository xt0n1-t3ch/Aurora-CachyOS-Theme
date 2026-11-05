// SPDX-License-Identifier: GPL-3.0-only
// The macOS switch of the detail panels' headers.
import QtQuick

Item {
    id: control

    // Set by the owner: a click only emits toggled and never flips the switch itself.
    property bool checked
    signal toggled()

    width: 38
    height: 22
    Accessible.role: Accessible.CheckBox
    Accessible.checked: checked

    Rectangle {
        anchors.fill: parent
        radius: height / 2
        color: control.checked ? Tokens.accent : Qt.rgba(1, 1, 1, 0.20)

        Behavior on color {
            ColorAnimation { duration: Tokens.fast }
        }
    }

    Rectangle {
        width: 18
        height: 18
        radius: 9
        y: 2
        x: control.checked ? parent.width - width - 2 : 2
        color: "white"

        Behavior on x {
            NumberAnimation { duration: Tokens.fast; easing.type: Easing.OutCubic }
        }
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onClicked: control.toggled()
    }
}
