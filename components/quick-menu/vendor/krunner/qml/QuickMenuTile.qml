/*
    SPDX-FileCopyrightText: 2026 xt0n1-t3ch
    SPDX-License-Identifier: GPL-2.0-or-later
*/
// One app of the Applications view: its large icon over its name, as Tahoe's Spotlight shows apps. One soft rounded
// highlight marks the selected app, which the pointer takes only when it moves (an app that happens to lie under a
// resting pointer when Quick Menu opens is not selected, so Return cannot launch it by surprise); the icon dips a
// little while pressed. A draggable tile can be dragged onto another app of its section: its icon follows the
// pointer, the app under it lights up, and the drop moves the app there.
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Item {
    id: tile

    property var icon
    property string label
    property bool current: false
    property int iconSize: Kirigami.Units.iconSizes.huge
    // Its place in its section, whether it may be dragged, and the key only tiles of the same section share.
    property int position: -1
    property bool draggable: false
    property string dropKey: ""
    readonly property bool dragging: pointer.drag.active
    readonly property bool dropTarget: drop.containsDrag && drop.drag.source !== tile

    signal activated()
    // An app of the same section, at `from`, was dropped on this one, at `to`.
    signal moved(int from, int to)
    // The pointer moved over the tile, at this global position (QuickMenuApps tells real motion from the hover
    // events Qt sends when the content moves under a resting pointer).
    signal pointed(point position)
    // A right-click asked for the app's menu, at this global position.
    signal menuRequested(point position)

    implicitHeight: Kirigami.Units.largeSpacing + iconSize + Kirigami.Units.mediumSpacing + name.implicitHeight
                    + Kirigami.Units.largeSpacing
    // The dragged icon draws above the tiles after it in the grid.
    z: dragging ? 10 : 0

    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.onPressAction: tile.activated()

    Rectangle {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.smallSpacing / 2
        radius: Kirigami.Units.cornerRadius * 2.5
        color: Qt.rgba(1, 1, 1, tile.dropTarget ? 0.22 : tile.current ? 0.16 : 0)
        Behavior on color {
            ColorAnimation { duration: Kirigami.Units.shortDuration }
        }
    }

    Kirigami.Icon {
        id: image
        anchors.horizontalCenter: parent.horizontalCenter
        y: Kirigami.Units.largeSpacing
        width: tile.iconSize
        height: tile.iconSize
        source: tile.icon
        animated: false
        opacity: tile.dragging ? 0.35 : 1
        scale: pointer.pressed ? 0.92 : 1
        Behavior on scale {
            NumberAnimation { duration: Kirigami.Units.shortDuration; easing.type: Easing.OutCubic }
        }
    }

    QQC2.Label {
        id: name
        anchors.top: image.bottom
        anchors.topMargin: Kirigami.Units.mediumSpacing
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 2 * Kirigami.Units.smallSpacing
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        maximumLineCount: 1
        textFormat: Text.PlainText
        text: tile.label
        color: Qt.rgba(1, 1, 1, 0.92)
    }

    // What follows the pointer while the app is dragged.
    Kirigami.Icon {
        id: ghost
        x: image.x
        y: image.y
        width: tile.iconSize
        height: tile.iconSize
        visible: tile.dragging
        source: tile.icon
        animated: false
        Drag.active: tile.dragging
        Drag.source: tile
        Drag.keys: [tile.dropKey]
        Drag.hotSpot.x: width / 2
        Drag.hotSpot.y: height / 2
    }

    DropArea {
        id: drop
        anchors.fill: parent
        keys: [tile.dropKey]
        onDropped: event => {
            if (event.source !== tile && event.source.position >= 0) {
                event.accept()
                tile.moved(event.source.position, tile.position)
            }
        }
    }

    MouseArea {
        id: pointer
        // A drag is a move, not a click: the release that ends it must not launch the app.
        property bool dragged: false
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        cursorShape: tile.dragging ? Qt.ClosedHandCursor : Qt.PointingHandCursor
        // Only the left button drags; the right one opens the menu.
        drag.target: tile.draggable && pressedButtons === Qt.LeftButton ? ghost : null
        // Inside the scrolling view a drag on a tile moves the app; the wheel and the scroll bar still scroll.
        preventStealing: tile.draggable
        onPressed: mouse => {
            dragged = false
            if (mouse.button === Qt.RightButton) {
                tile.menuRequested(mapToGlobal(mouse.x, mouse.y))
            }
        }
        onPositionChanged: mouse => {
            if (drag.active) {
                dragged = true
            }
            tile.pointed(mapToGlobal(mouse.x, mouse.y))
        }
        onReleased: {
            if (dragged) {
                ghost.Drag.drop()
            }
            ghost.x = image.x
            ghost.y = image.y
        }
        onClicked: mouse => {
            if (mouse.button === Qt.LeftButton && !dragged) {
                tile.activated()
            }
        }
    }
}
