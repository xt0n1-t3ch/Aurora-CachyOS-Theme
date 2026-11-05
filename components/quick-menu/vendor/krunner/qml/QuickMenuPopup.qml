/*
    SPDX-FileCopyrightText: 2026 xt0n1-t3ch
    SPDX-License-Identifier: GPL-2.0-or-later
*/
// The menu an app of the Applications view opens on a right-click or the Menu key, drawn like macOS Tahoe's context
// menus: rounded dark glass with a soft shadow, a symbolic icon before each entry, the accent color under the
// pointer. It lists the items quickmenu.js's menuItems() builds. The arrow keys move through it, Return chooses,
// Escape or a click elsewhere closes it.
import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

QQC2.Popup {
    id: menu

    // Each item is {label, icon, fallback, action}, {title: true, label} or {separator: true}.
    property var items: []
    // The app the menu is for, passed back with the chosen action.
    property var entry: null
    // The highlighted item, or -1.
    property int currentIndex: -1

    readonly property int rowHeight: Math.round(Kirigami.Units.gridUnit * 1.4)
    readonly property int titleHeight: Math.round(Kirigami.Units.gridUnit * 1.2)
    readonly property int separatorHeight: Kirigami.Units.smallSpacing * 2 + 1
    readonly property int iconSize: Kirigami.Units.iconSizes.small
    // Space between the row's edge and its icon, and between the icon and the label.
    readonly property int rowPadding: Kirigami.Units.mediumSpacing + Kirigami.Units.smallSpacing
    readonly property int iconGap: Kirigami.Units.mediumSpacing + Kirigami.Units.smallSpacing
    readonly property TextMetrics metrics: TextMetrics {
        font: Kirigami.Theme.defaultFont
    }

    signal chosen(var entry, var action)

    padding: Kirigami.Units.smallSpacing + 1
    // Qt keeps the menu inside the window by this much, should the placement below not manage it.
    margins: Kirigami.Units.smallSpacing
    focus: true
    modal: false
    closePolicy: QQC2.Popup.CloseOnEscape | QQC2.Popup.CloseOnPressOutside

    // The menu's size for `list`, known before it opens, so openAt() can place it.
    function measure(list) {
        let widest = 0
        let height = 0
        for (const item of list) {
            if (item.separator) {
                height += separatorHeight
                continue
            }
            metrics.text = item.label
            widest = Math.max(widest, metrics.advanceWidth)
            height += item.title ? titleHeight : rowHeight
        }
        const width = Math.max(Kirigami.Units.gridUnit * 9, Math.ceil(widest) + iconSize + iconGap + rowPadding * 2)
        return Qt.size(width + leftPadding + rightPadding, height + topPadding + bottomPadding)
    }

    // Opens the menu for `app` at (x, y) of its parent: below and to the right of that point, as macOS does, or to
    // its left or above it where the parent ends. `byKey` highlights the first entry, as Windows does for a menu
    // opened from the keyboard.
    function openAt(app, list, x, y, byKey) {
        entry = app
        items = list
        currentIndex = byKey ? 0 : -1
        const size = measure(list)
        const gap = Kirigami.Units.smallSpacing
        width = size.width
        height = size.height
        const left = x + size.width + gap <= parent.width ? x : x - size.width
        const top = y + size.height + gap <= parent.height ? y : y - size.height
        menu.x = Math.max(gap, Math.min(left, parent.width - size.width - gap))
        menu.y = Math.max(gap, Math.min(top, parent.height - size.height - gap))
        open()
    }

    // Whether the item at `index` can be chosen: separators and titles cannot.
    function choosable(index) {
        const item = items[index]
        return !!item && !item.separator && !item.title
    }

    function step(direction) {
        for (let i = 1; i <= items.length; ++i) {
            const next = ((currentIndex + direction * i) % items.length + items.length) % items.length
            if (choosable(next)) {
                currentIndex = next
                return
            }
        }
    }

    function choose(index) {
        if (choosable(index)) {
            close()
            chosen(entry, items[index].action)
        }
    }

    enter: Transition {
        NumberAnimation {
            property: "opacity"; from: 0; to: 1
            duration: Kirigami.Units.shortDuration; easing.type: Easing.OutCubic
        }
    }
    exit: Transition {
        NumberAnimation {
            property: "opacity"; from: 1; to: 0
            duration: Kirigami.Units.shortDuration; easing.type: Easing.InCubic
        }
    }

    background: Kirigami.ShadowedRectangle {
        radius: Math.round(Kirigami.Units.gridUnit * 0.7)
        color: Qt.rgba(0.16, 0.155, 0.19, 0.97)
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.13)
        shadow.size: Kirigami.Units.gridUnit * 1.5
        shadow.yOffset: Kirigami.Units.smallSpacing * 2
        shadow.color: Qt.rgba(0, 0, 0, 0.45)
    }

    contentItem: Column {
        focus: true
        Keys.onUpPressed: menu.step(-1)
        Keys.onDownPressed: menu.step(1)
        Keys.onReturnPressed: menu.choose(menu.currentIndex)
        Keys.onEnterPressed: menu.choose(menu.currentIndex)

        Repeater {
            model: menu.items

            delegate: Item {
                id: row

                required property var modelData
                required property int index
                readonly property bool separator: modelData.separator === true
                readonly property bool title: modelData.title === true
                readonly property bool highlighted: menu.currentIndex === index

                width: menu.availableWidth
                height: separator ? menu.separatorHeight : title ? menu.titleHeight : menu.rowHeight

                Rectangle {
                    visible: row.separator
                    anchors.verticalCenter: parent.verticalCenter
                    x: menu.rowPadding
                    width: parent.width - x * 2
                    height: 1
                    color: Qt.rgba(1, 1, 1, 0.12)
                }

                Rectangle {
                    visible: !row.separator && row.highlighted
                    anchors.fill: parent
                    radius: Math.round(Kirigami.Units.gridUnit * 0.4)
                    color: Kirigami.Theme.highlightColor
                }

                // A heading among Plasma's actions, such as "Recent Files": small and dim, like macOS's section titles.
                QQC2.Label {
                    visible: row.title
                    anchors.verticalCenter: parent.verticalCenter
                    x: menu.rowPadding
                    width: parent.width - x * 2
                    text: row.modelData.label || ""
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    font.weight: Font.DemiBold
                    color: Qt.rgba(1, 1, 1, 0.5)
                }

                Kirigami.Icon {
                    id: icon
                    visible: !row.separator && !row.title
                    anchors.verticalCenter: parent.verticalCenter
                    x: menu.rowPadding
                    width: menu.iconSize
                    height: menu.iconSize
                    source: row.modelData.icon || ""
                    fallback: row.modelData.fallback || ""
                    isMask: true
                    color: label.color
                }

                QQC2.Label {
                    id: label
                    visible: !row.separator && !row.title
                    anchors.verticalCenter: parent.verticalCenter
                    x: icon.x + icon.width + menu.iconGap
                    width: parent.width - x - menu.rowPadding
                    text: row.modelData.label || ""
                    elide: Text.ElideRight
                    textFormat: Text.PlainText
                    color: row.highlighted ? Kirigami.Theme.highlightedTextColor : Qt.rgba(1, 1, 1, 0.92)
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: menu.choosable(row.index)
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onEntered: menu.currentIndex = row.index
                    onExited: if (menu.currentIndex === row.index) menu.currentIndex = -1
                    onClicked: menu.choose(row.index)
                }

                Accessible.role: row.separator ? Accessible.Separator
                               : row.title ? Accessible.StaticText : Accessible.MenuItem
                Accessible.name: row.modelData.label || ""
                Accessible.onPressAction: menu.choose(row.index)
            }
        }
    }

    onOpened: contentItem.forceActiveFocus()
}
