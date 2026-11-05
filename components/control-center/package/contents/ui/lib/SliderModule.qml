// SPDX-License-Identifier: GPL-3.0-only
// A 4 x 1 module with a title and a thin glass slider, like Tahoe's Display and Sound: no knob, the bar fills
// from the left and thickens while the pointer is on it. value runs from 0 to 1.
import QtQuick

Glass {
    id: module

    property string title
    // From 0 to 1. The slider never sets it: the owner follows moved and sets value in turn.
    property real value: 0
    // The glyphs at the two ends of the bar. The first is a button when startClickable is true (Sound mutes with it).
    property string startGlyph
    property string endGlyph
    property bool startClickable: false
    // Adds a round button after the bar (Sound lists its outputs with it).
    property bool outputButton: false
    // moved: the bar was dragged or scrolled to this value. The other two signals are the two buttons.
    signal moved(real value)
    signal startClicked()
    signal outputClicked()

    radius: 24
    Accessible.role: Accessible.Slider
    Accessible.name: title

    Text {
        x: 16
        y: 10
        text: module.title
        color: Tokens.text
        font.family: Tokens.font
        font.pixelSize: Tokens.titleSize
        font.weight: Font.DemiBold
    }

    Item {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 16
        anchors.rightMargin: module.outputButton ? 9 : 16
        anchors.bottomMargin: module.outputButton ? 6 : 9
        height: module.outputButton ? 28 : 22

        Symbol {
            id: startIcon
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 17
            height: 17
            name: module.startGlyph

            MouseArea {
                anchors.fill: parent
                anchors.margins: -5
                enabled: module.startClickable
                cursorShape: Qt.PointingHandCursor
                onClicked: module.startClicked()
            }
        }

        Item {
            id: track
            anchors.left: startIcon.right
            anchors.leftMargin: 10
            anchors.right: endIcon.left
            anchors.rightMargin: 10
            anchors.verticalCenter: parent.verticalCenter
            height: 22
            readonly property bool engaged: drag.pressed || drag.containsMouse

            Rectangle {
                id: groove
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: track.engaged ? 7 : 5
                radius: height / 2
                color: Tokens.track

                Behavior on height {
                    NumberAnimation { duration: Tokens.fast; easing.type: Easing.OutCubic }
                }

                Rectangle {
                    visible: module.value > 0
                    width: Math.max(groove.height, groove.width * Math.min(1, module.value))
                    height: parent.height
                    radius: parent.radius
                    color: Tokens.trackFill
                }
            }

            MouseArea {
                id: drag
                anchors.fill: parent
                // The hit area is taller than the thin bar.
                anchors.topMargin: -8
                anchors.bottomMargin: -8
                hoverEnabled: true
                preventStealing: true
                cursorShape: Qt.PointingHandCursor

                function moveTo(x) {
                    module.moved(Math.max(0, Math.min(1, x / track.width)))
                }

                onPressed: mouse => moveTo(mouse.x)
                onPositionChanged: mouse => {
                    if (pressed) {
                        moveTo(mouse.x)
                    }
                }
                onWheel: wheel => module.moved(Math.max(0, Math.min(1, module.value + (wheel.angleDelta.y > 0 ? 0.05 : -0.05))))
            }
        }

        Symbol {
            id: endIcon
            anchors.right: output.visible ? output.left : parent.right
            anchors.rightMargin: output.visible ? 10 : 0
            anchors.verticalCenter: parent.verticalCenter
            width: 21
            height: 21
            name: module.endGlyph
        }

        Glass {
            id: output
            visible: module.outputButton
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 28
            height: 28
            frosted: false
            shadowed: false
            tint: Tokens.softTint
            lift: outputArea.pressed ? 1 : outputArea.containsMouse ? 0.5 : 0

            Symbol {
                anchors.centerIn: parent
                width: 17
                height: 17
                name: "output"
            }

            MouseArea {
                id: outputArea
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: module.outputClicked()
            }
        }
    }
}
