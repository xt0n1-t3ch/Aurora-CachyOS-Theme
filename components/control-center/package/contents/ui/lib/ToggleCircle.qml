// SPDX-License-Identifier: GPL-3.0-only
// The round switch at the start of a pill: white with a colored glyph when on, faint glass when off.
import QtQuick

Glass {
    id: circle

    property string glyph
    property bool on
    // The Symbol tone of the glyph while on, drawn on the white circle.
    property string onTone: "blue"
    signal clicked()

    width: 36
    height: 36
    frosted: false
    shadowed: false
    tint: on ? Tokens.onTint : Tokens.softTint
    lift: area.pressed ? 1 : area.containsMouse ? 0.5 : 0
    opacity: enabled ? 1 : 0.45
    Accessible.role: Accessible.CheckBox
    Accessible.checked: on

    Symbol {
        anchors.centerIn: parent
        name: circle.glyph
        tone: circle.on ? circle.onTone : "light"
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: circle.clicked()
    }
}
