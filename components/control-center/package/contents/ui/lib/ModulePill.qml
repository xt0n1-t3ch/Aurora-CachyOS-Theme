// SPDX-License-Identifier: GPL-3.0-only
// A 2 x 1 Control Center module: the round switch toggles, the rest of the pill opens its details, as on macOS.
import QtQuick
import "../js/cc.js" as CC

Glass {
    id: pill

    // The round switch: its glyph, its state, the glyph's Symbol tone while on, and whether it accepts clicks.
    property alias glyph: toggle.glyph
    property alias on: toggle.on
    property alias onTone: toggle.onTone
    property alias toggleEnabled: toggle.enabled
    property string title
    property string subtitle
    // toggled: the round switch was clicked. opened: the rest of the pill was clicked.
    signal toggled()
    signal opened()

    lift: body.pressed ? 1 : body.containsMouse ? 0.5 : 0
    Accessible.role: Accessible.Button
    Accessible.name: title
    Accessible.description: subtitle

    MouseArea {
        id: body
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: pill.opened()
    }

    ToggleCircle {
        id: toggle
        x: (pill.height - width) / 2
        anchors.verticalCenter: parent.verticalCenter
        onClicked: pill.toggled()
    }

    Column {
        readonly property var place: CC.pillText(pill.width, pill.height, toggle.width)
        x: place.x
        width: place.width
        anchors.verticalCenter: parent.verticalCenter
        spacing: 1

        Text {
            width: parent.width
            text: pill.title
            color: Tokens.text
            font.family: Tokens.font
            font.pixelSize: Tokens.titleSize
            font.weight: Font.DemiBold
            // Hinted text runs a little wider; shrink a long title a step before eliding it.
            fontSizeMode: Text.HorizontalFit
            minimumPixelSize: Tokens.titleSize - 2
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: text.length > 0
            text: pill.subtitle
            color: Tokens.secondaryText
            font.family: Tokens.font
            font.pixelSize: Tokens.subtitleSize
            elide: Text.ElideRight
        }
    }
}
