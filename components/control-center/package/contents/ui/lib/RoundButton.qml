// SPDX-License-Identifier: GPL-3.0-only
// A 1 x 1 Control Center control: a glass circle with one glyph.
import QtQuick
import org.kde.plasma.components as PlasmaComponents

Glass {
    id: button

    property string glyph
    // The tooltip and the accessible name.
    property string label
    // When on, the circle turns white and the glyph takes onTone, a Symbol tone.
    property bool on: false
    property string onTone: "dark"
    signal clicked()

    tint: on ? Tokens.onTint : Tokens.glassTint
    lift: area.pressed ? 1 : area.containsMouse ? 0.5 : 0
    Accessible.role: Accessible.Button
    Accessible.name: label

    Symbol {
        anchors.centerIn: parent
        width: 24
        height: 24
        name: button.glyph
        tone: button.on ? button.onTone : "light"
    }

    MouseArea {
        id: area
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: button.clicked()
    }

    PlasmaComponents.ToolTip {
        text: button.label
        visible: area.containsMouse && text.length > 0
        delay: 700
    }
}
