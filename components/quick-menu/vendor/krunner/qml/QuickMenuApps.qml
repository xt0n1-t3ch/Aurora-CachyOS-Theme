/*
    SPDX-FileCopyrightText: 2026 xt0n1-t3ch
    SPDX-License-Identifier: GPL-2.0-or-later
*/
// Quick Menu's Applications view: Suggestions (the apps used most, from Plasma's activity statistics), then every
// app of Plasma's menu in Tahoe's sections, one row each until "Show More". The menu and the statistics are the
// models Kickoff uses (org.kde.plasma.private.kicker), so new apps and launches show up as they do there. An app
// dragged onto another of its section takes that one's place, and the section keeps that order. A right-click (or the
// Menu key) opens an app's menu: Open, its jump list, Pin to Top (the Pinned section above Suggestions), Pin to Dock,
// Remove from Suggestions, and Plasma's Edit Application and Uninstall. The grid button beside ⋯ shows All Applications:
// every app under its first letter, like Windows' All apps, with Back where a section has Show More.
import QtCore
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.private.kicker as Kicker
import org.kde.plasma.workspace.dbus as DBus
import "quickmenu.js" as QuickMenu

Flickable {
    id: view

    property int columns: 5
    property int tileWidth: Math.round(iconSize * 1.9)
    property int iconSize: Kirigami.Units.iconSizes.huge
    property int sidePadding: Kirigami.Units.gridUnit

    property var sections: []
    // All Applications: every app under its first letter instead of the sections.
    property bool allApps: false
    // Section title -> shown in full.
    property var expanded: ({})
    // {section, index} of the app the arrow keys stand on, or null while the search field has them.
    property var current: null
    // The view scrolls to the current app only when the keys moved it; the pointer is already on it.
    property bool followKeys: false
    // Where the pointer last was: Qt sends hover moves when the grid moves under a resting pointer, and only a
    // pointer that really moved may take the selection.
    property var lastPointer: null
    readonly property bool hasContent: sections.length > 0

    signal launched()

    // Kicker::FavoriteIdRole, the desktop file id, and Kicker::ActionListRole, the app's actions
    // (plasma-workspace applets/kicker/actionlist.h, Plasma 6.8: Qt::UserRole + 3 and + 9).
    readonly property int favoriteIdRole: Qt.UserRole + 3
    readonly property int actionListRole: Qt.UserRole + 9
    // The launchers of the Dock (the task manager on the bottom panel), read from Plasma when Quick Menu opens.
    property var dockLaunchers: []

    contentWidth: width
    contentHeight: content.implicitHeight
    clip: true
    boundsBehavior: Flickable.StopAtBounds
    QQC2.ScrollBar.vertical: QuickMenuScrollBar {}

    // The order the apps of each section were dragged into, {section title: [app ids]}, kept in
    // ~/.config/aurora/quick-menu.conf. Suggestions are ordered by use and cannot be dragged.
    Settings {
        id: saved
        location: StandardPaths.writableLocation(StandardPaths.GenericConfigLocation) + "/aurora/quick-menu.conf"
        category: "Applications"
        property string order: "{}"
        // The apps pinned with "Pin to Top", as a JSON list of app ids in their order.
        property string pinned: "[]"
    }

    function savedPins() {
        try {
            const pins = JSON.parse(saved.pinned)
            return Array.isArray(pins) ? pins : []
        } catch (error) {
            return []
        }
    }

    function savedOrders() {
        try {
            return JSON.parse(saved.order) || {}
        } catch (error) {
            return {}
        }
    }

    // The app at `from` was dropped on the app at `to` of the same section: it takes that place, for good.
    function moveApp(sectionIndex, from, to) {
        const section = sections[sectionIndex]
        if (!section || !section.reorderable) {
            return
        }
        if (section.pinned) {
            saved.pinned = JSON.stringify(QuickMenu.reorder(section.apps.map(entry => entry.id), from, to))
            saved.sync()
            current = null
            rebuild()
            return
        }
        const orders = savedOrders()
        orders[section.title] = QuickMenu.reorder(section.apps.map(entry => entry.id), from, to)
        saved.order = JSON.stringify(orders)
        saved.sync()
        current = null
        rebuild()
    }

    Kicker.RootModel {
        id: menu
        appNameFormat: 0
        flat: true
        sorted: true
        showSeparators: false
        showTopLevelItems: false
        showAllApps: false
        showAllAppsCategorized: false
        showRecentApps: false
        showRecentDocs: false
        showRecentFolders: false
        showPowerSession: false
        showFavoritesPlaceholder: false
        highlightNewlyInstalledApps: false
        onCountChanged: Qt.callLater(view.rebuild)
        onModelReset: Qt.callLater(view.rebuild)
    }
    Kicker.RecentUsageModel {
        id: popular
        ordering: 1 // Popular
        shownItems: Kicker.RecentUsageModel.OnlyApps
        // The statistics arrive after start-up and change with every launch.
        onCountChanged: Qt.callLater(view.rebuild)
    }

    function entriesOf(model, limit) {
        const entries = []
        for (let row = 0; model && row < model.count && entries.length < limit; ++row) {
            const index = model.index(row, 0)
            const name = String(model.data(index, Qt.DisplayRole) || "")
            if (name.length > 0) {
                entries.push({id: String(model.data(index, favoriteIdRole) || name), name: name,
                              icon: model.data(index, Qt.DecorationRole), model: model, row: row})
            }
        }
        return entries
    }

    function rebuild() {
        const categories = []
        for (let row = 0; row < menu.count; ++row) {
            const index = menu.index(row, 0)
            categories.push({icon: String(menu.data(index, Qt.DecorationRole) || ""), name: String(menu.data(index, Qt.DisplayRole) || ""),
                             apps: entriesOf(menu.modelForRow(row), Infinity)})
        }
        if (allApps) {
            sections = QuickMenu.alphabeticalSections(categories)
            if (current && current.section >= sections.length) {
                current = null
            }
            return
        }
        const built = QuickMenu.buildSections(categories, savedOrders())
        // Three rows at most behind Show More: the apps used most, not a second full list.
        const suggested = entriesOf(popular, 3 * columns)
        if (suggested.length > 0) {
            built.unshift({title: i18nc("@title Quick Menu section of the apps used most", "Suggestions"), apps: suggested,
                           suggestions: true})
        }
        const pinned = QuickMenu.pinnedSection(categories, savedPins())
        if (pinned) {
            pinned.title = i18nc("@title Quick Menu section of the apps pinned to the top", "Pinned")
            built.unshift(pinned)
        }
        sections = built
        if (current && current.section >= sections.length) {
            current = null
        }
    }

    // Back to a fresh view each time Quick Menu opens: all sections folded, nothing selected, at the top.
    function reset() {
        allApps = false
        expanded = ({})
        current = null
        lastPointer = null
        contentY = 0
        appMenu.close()
        rebuild()
        readDock()
    }

    // Asks Plasma for the Dock's launchers, so the menu knows whether to pin or unpin.
    function readDock() {
        shell(QuickMenu.dockReadScript(), text => dockLaunchers = QuickMenu.launchers(text))
    }

    // Runs a Plasma shell script (org.kde.PlasmaShell.evaluateScript) and hands what it printed to `done`.
    function shell(script, done) {
        if (!script) {
            return
        }
        DBus.SessionBus.asyncCall({service: "org.kde.plasmashell", path: "/PlasmaShell", iface: "org.kde.PlasmaShell",
                                   member: "evaluateScript", arguments: [script]},
                                  reply => done(String(reply.value && reply.value.value !== undefined ? reply.value.value : reply.value)),
                                  error => console.warn("Quick Menu: Plasma did not run the script:", error.message))
    }

    // The app shown at `appIndex` of section `sectionIndex`, or null.
    function entryAt(sectionIndex, appIndex) {
        const section = sections[sectionIndex]
        return section ? QuickMenu.visibleApps(section, !!expanded[section.title], columns)[appIndex] || null : null
    }

    // Opens the menu of an app at (x, y) in this view's coordinates; `byKey` when the keyboard asked for it.
    function openMenu(sectionIndex, appIndex, x, y, byKey) {
        const entry = entryAt(sectionIndex, appIndex)
        if (!entry) {
            return
        }
        const actions = entry.model.data(entry.model.index(entry.row, 0), actionListRole)
        const items = QuickMenu.menuItems({suggested: entry.model === popular, pinned: savedPins().indexOf(entry.id) >= 0,
                                           inDock: dockLaunchers.indexOf(entry.id) >= 0, actions: actions || []})
        current = {section: sectionIndex, index: appIndex}
        appMenu.openAt(entry, items, x, y, byKey === true)
    }

    // The Menu key or Shift+F10 opens the menu of the app the arrow keys stand on, under it.
    function openMenuForCurrent() {
        if (!current) {
            return false
        }
        const tile = tileAt(current.section, current.index)
        const corner = tile ? tile.mapToItem(view, tile.width / 2, tile.height * 0.8) : Qt.point(width / 2, height / 2)
        openMenu(current.section, current.index, corner.x, corner.y, true)
        return true
    }

    function tileAt(sectionIndex, appIndex) {
        const sectionItem = sectionRepeater.itemAt(sectionIndex)
        return sectionItem ? sectionItem.tileAt(appIndex) : null
    }

    function runMenuAction(entry, action) {
        switch (action.kind) {
        case "open":
            if (entry.model.trigger(entry.row, "", null)) {
                launched()
            }
            break
        case "plasma":
            if (entry.model.trigger(entry.row, action.action.actionId, action.action.actionArgument)) {
                launched()
            }
            break
        case "pin":
            saved.pinned = JSON.stringify(QuickMenu.togglePin(savedPins(), entry.id))
            saved.sync()
            current = null
            rebuild()
            break
        case "dock":
            shell(QuickMenu.dockScript(entry.id, dockLaunchers.indexOf(entry.id) < 0), () => readDock())
            break
        case "forget":
            // Kicker's "Forget Application": the app leaves Suggestions until it is used again.
            entry.model.trigger(entry.row, "forget", null)
            current = null
            break
        }
    }

    // The menu stays where it opened, so scrolling the view closes it.
    onMovingChanged: if (moving) appMenu.close()

    QuickMenuPopup {
        id: appMenu
        parent: view
        onChosen: (entry, action) => view.runMenuAction(entry, action)
    }

    function visibleCounts() {
        return sections.map(section => QuickMenu.visibleApps(section, !!expanded[section.title], columns).length)
    }

    // Returns false when the keys leave the grid upwards, back to the search field.
    function moveCurrent(key) {
        followKeys = true
        current = QuickMenu.move(visibleCounts(), current, key, columns)
        return current !== null
    }

    function launch(sectionIndex, appIndex) {
        const section = sections[sectionIndex]
        const entry = section ? QuickMenu.visibleApps(section, !!expanded[section.title], columns)[appIndex] : null
        if (entry && entry.model.trigger(entry.row, "", null)) {
            launched()
        }
    }

    function launchCurrent() {
        if (current) {
            launch(current.section, current.index)
        }
    }

    // Switches between the sections and All Applications, from the top of the list.
    function showAllApps(all) {
        allApps = all
        current = null
        contentY = 0
        rebuild()
    }

    function toggle(title) {
        const next = Object.assign({}, expanded)
        next[title] = !next[title]
        expanded = next
        // Folding a section can hide the app the keys stood on: keep them on its last shown app.
        if (current && sections[current.section].title === title) {
            current = {section: current.section, index: Math.min(current.index, visibleCounts()[current.section] - 1)}
        }
    }

    function reveal(item) {
        const top = item.mapToItem(content, 0, 0).y
        if (top < contentY) {
            contentY = Math.max(0, top - Kirigami.Units.smallSpacing)
        } else if (top + item.height > contentY + height) {
            contentY = Math.min(contentHeight - height, top + item.height - height + Kirigami.Units.smallSpacing)
        }
    }

    Behavior on contentY {
        enabled: !view.moving
        NumberAnimation { duration: Kirigami.Units.shortDuration; easing.type: Easing.OutCubic }
    }

    Column {
        id: content
        width: view.width
        topPadding: Kirigami.Units.mediumSpacing
        bottomPadding: Kirigami.Units.mediumSpacing

        // All Applications' heading, drawn like a section's: its title, and Back where a section has Show More. The
        // way in is the grid button beside ⋯ in the search row.
        RowLayout {
            x: view.sidePadding
            width: content.width - 2 * view.sidePadding
            height: Kirigami.Units.gridUnit * 2.2
            visible: view.allApps

            QQC2.Label {
                Layout.fillWidth: true
                text: i18nc("@title Quick Menu's list of every app", "All Applications")
                font.weight: Font.Bold
                color: Qt.rgba(1, 1, 1, 0.95)
                elide: Text.ElideRight
                textFormat: Text.PlainText
            }
            QQC2.Label {
                text: i18nc("@action:button back from Quick Menu's list of every app", "Back")
                font.weight: Font.DemiBold
                color: back.hovered ? Qt.lighter(Kirigami.Theme.highlightColor, 1.6) : Qt.lighter(Kirigami.Theme.highlightColor, 1.35)
                textFormat: Text.PlainText
                Accessible.role: Accessible.Button
                Accessible.name: text
                Accessible.onPressAction: view.showAllApps(false)

                HoverHandler {
                    id: back
                    cursorShape: Qt.PointingHandCursor
                }
                TapHandler {
                    onTapped: view.showAllApps(false)
                }
            }
        }

        Repeater {
            id: sectionRepeater
            model: view.sections

            delegate: Column {
                id: section

                required property var modelData
                required property int index
                readonly property bool open: !!view.expanded[modelData.title]

                function tileAt(appIndex) {
                    return tiles.itemAt(appIndex)
                }

                width: content.width

                Rectangle {
                    // Under All Applications' heading the first letter has a divider too.
                    visible: section.index > 0 || view.allApps
                    x: view.sidePadding
                    width: parent.width - 2 * view.sidePadding
                    height: 1
                    color: Qt.rgba(1, 1, 1, 0.12)
                }

                RowLayout {
                    x: view.sidePadding
                    width: parent.width - 2 * view.sidePadding
                    height: Kirigami.Units.gridUnit * 2.2

                    QQC2.Label {
                        Layout.fillWidth: true
                        text: section.modelData.title
                        font.weight: Font.Bold
                        color: Qt.rgba(1, 1, 1, 0.95)
                        elide: Text.ElideRight
                        textFormat: Text.PlainText
                    }
                    QQC2.Label {
                        visible: QuickMenu.canExpand(section.modelData, view.columns)
                        text: section.open ? i18nc("@action:button fold a Quick Menu section", "Show Less")
                                           : i18nc("@action:button unfold a Quick Menu section", "Show More")
                        font.weight: Font.DemiBold
                        color: more.hovered ? Qt.lighter(Kirigami.Theme.highlightColor, 1.6) : Qt.lighter(Kirigami.Theme.highlightColor, 1.35)
                        textFormat: Text.PlainText
                        Accessible.role: Accessible.Button
                        Accessible.name: text
                        Accessible.onPressAction: view.toggle(section.modelData.title)

                        HoverHandler {
                            id: more
                            cursorShape: Qt.PointingHandCursor
                        }
                        TapHandler {
                            onTapped: view.toggle(section.modelData.title)
                        }
                    }
                }

                Flow {
                    x: view.sidePadding
                    width: view.columns * view.tileWidth

                    Repeater {
                        id: tiles
                        model: QuickMenu.visibleApps(section.modelData, section.open, view.columns)

                        delegate: QuickMenuTile {
                            id: tile
                            required property var modelData
                            required property int index
                            width: view.tileWidth
                            iconSize: view.iconSize
                            icon: modelData.icon
                            label: modelData.name
                            position: index
                            draggable: section.modelData.reorderable === true
                            dropKey: "aurora-quick-menu-section-" + section.index
                            onMoved: (from, to) => view.moveApp(section.index, from, to)
                            onMenuRequested: position => {
                                const point = view.mapFromGlobal(position.x, position.y)
                                view.openMenu(section.index, index, point.x, point.y)
                            }
                            current: !!view.current && view.current.section === section.index && view.current.index === index
                            onCurrentChanged: if (current && view.followKeys) view.reveal(tile)
                            onActivated: view.launch(section.index, index)
                            onPointed: position => {
                                const moved = view.lastPointer && (position.x !== view.lastPointer.x || position.y !== view.lastPointer.y)
                                view.lastPointer = position
                                if (moved) {
                                    view.followKeys = false
                                    view.current = {section: section.index, index: index}
                                }
                            }
                        }
                    }
                }

                Item {
                    width: 1
                    height: Kirigami.Units.mediumSpacing
                }
            }
        }
    }
}
