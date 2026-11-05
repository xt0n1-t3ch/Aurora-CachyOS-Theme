// SPDX-License-Identifier: GPL-3.0-only
// Now Playing (2 x 2): the current MPRIS player's artwork, title, artist and transport.
import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.plasma.private.mpris as Mpris
import "../lib"
import "../js/cc.js" as CC

Glass {
    id: media

    property Item controlCenter
    readonly property var player: mpris.currentPlayer
    readonly property var summary: CC.mediaSummary(player ? {track: player.track, artist: player.artist, identity: player.identity} : null)
    readonly property bool playing: !!player && player.playbackStatus === Mpris.PlaybackStatus.Playing
    readonly property bool canControl: !!player && player.canControl

    radius: 26
    lift: body.pressed ? 1 : body.containsMouse && summary.hasPlayer ? 0.5 : 0

    Mpris.Mpris2Model { id: mpris }

    // A click on the card brings the player's window forward.
    MouseArea {
        id: body
        anchors.fill: parent
        hoverEnabled: true
        enabled: media.summary.hasPlayer
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            if (media.player && media.player.canRaise) {
                media.controlCenter.close()
                media.player.Raise()
            }
        }
    }

    Rectangle {
        id: placeholder
        x: 14
        y: 14
        width: 44
        height: 44
        radius: 10
        color: Qt.rgba(1, 1, 1, 0.10)
        visible: art.status !== Image.Ready

        Symbol {
            anchors.centerIn: parent
            width: 22
            height: 22
            opacity: 0.55
            name: "music"
        }
    }

    Kirigami.ShadowedImage {
        id: art
        x: 14
        y: 14
        width: 44
        height: 44
        radius: 10
        source: media.player && media.player.artUrl ? media.player.artUrl : ""
        sourceSize: Qt.size(88, 88)
        fillMode: Image.PreserveAspectCrop
        visible: status === Image.Ready
        color: "transparent"
    }

    Column {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 14
        anchors.rightMargin: 12
        y: 66
        spacing: 1

        Text {
            width: parent.width
            text: media.summary.title
            color: Tokens.text
            font.family: Tokens.font
            font.pixelSize: Tokens.titleSize
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            visible: text.length > 0
            text: media.summary.artist
            color: Tokens.secondaryText
            font.family: Tokens.font
            font.pixelSize: Tokens.subtitleSize
            elide: Text.ElideRight
        }
    }

    Row {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 10
        spacing: 12
        opacity: media.canControl ? 1 : 0.4

        // One transport button, dimmed and inert while its action is not available.
        component Transport: Item {
            id: transport
            property string glyph
            property int size: 22
            property bool available: true
            signal clicked()
            width: 30
            height: 30
            Symbol {
                anchors.centerIn: parent
                width: transport.size
                height: transport.size
                name: transport.glyph
                opacity: transport.available ? (area.pressed ? 0.6 : 1) : 0.4
            }
            MouseArea {
                id: area
                anchors.fill: parent
                enabled: transport.available
                cursorShape: Qt.PointingHandCursor
                onClicked: transport.clicked()
            }
        }

        Transport {
            glyph: "backward"
            available: !!media.player && media.player.canGoPrevious
            onClicked: media.player.Previous()
        }
        Transport {
            glyph: media.playing ? "pause" : "play"
            size: 24
            available: media.canControl
            onClicked: media.player.PlayPause()
        }
        Transport {
            glyph: "forward"
            available: !!media.player && media.player.canGoNext
            onClicked: media.player.Next()
        }
    }
}
