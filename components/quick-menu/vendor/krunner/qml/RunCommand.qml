/*
    SPDX-FileCopyrightText: 2014 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2026 xt0n1-t3ch
    SPDX-License-Identifier: GPL-2.0-or-later
*/
// KRunner drawn as macOS Tahoe's Spotlight: one floating piece of glass with a large search field on top. With
// nothing typed it shows the Applications view (QuickMenuApps.qml); typing searches everything KRunner's runners
// find, listed the way Quick Menu lists results (QuickMenuResult.qml). Up in the empty field recalls past searches,
// and the ⋯ button keeps KRunner's own controls: keep open, usage help and its settings.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Templates as T
import QtQuick.Window

import org.kde.config // KAuthorized
import org.kde.kcmutils // KCMLauncher
import org.kde.plasma.components as PlasmaComponents3
import org.kde.plasma.extras as PlasmaExtras
import org.kde.milou as Milou
import org.kde.krunner.private.view
import org.kde.kirigami as Kirigami
import "quickmenu.js" as QuickMenu

Item {
    id: root

    required property RunnerWindow runnerWindow
    property string query
    property string singleRunner
    property bool showHistory: false
    property string priorSearch: ""
    property alias runnerManager: results.runnerManager

    // Tahoe's proportions, from the 64 px app icon: five columns of a little under two icons each.
    readonly property int columns: 5
    readonly property int iconSize: Kirigami.Units.iconSizes.huge
    readonly property int tileWidth: Math.round(iconSize * 1.9)
    readonly property int sidePadding: Math.round(Kirigami.Units.gridUnit * 1.1)
    readonly property int panelWidth: columns * tileWidth + 2 * sidePadding
    readonly property int headerHeight: Math.round(Kirigami.Units.gridUnit * 3.3)
    readonly property int shadowPad: Math.ceil(glass.shadowSize * 1.6)
    // The panel opens a quarter of the way down (view.cpp) and its body scrolls past this, so it ends on screen; the
    // account and power rail under the Applications view takes its height out of the body's.
    readonly property int bodyMaxHeight: Math.max(Kirigami.Units.gridUnit * 10, Math.round((Screen.height > 0 ? Screen.height : 1080) * 0.6) - headerHeight
                                                  - (mode === "apps" ? Math.round(Kirigami.Units.gridUnit * 3.2) : 0))

    readonly property string mode: query.length > 0 ? "results" : (showHistory ? "history" : "apps")

    function isKeyUp(event) {
        return (event.modifiers === Qt.NoModifier && event.key === Qt.Key_Up) || (event.modifiers & Qt.ControlModifier && event.key === Qt.Key_K)
    }
    function isKeyDown(event) {
        return (event.modifiers === Qt.NoModifier && event.key === Qt.Key_Down) || (event.modifiers & Qt.ControlModifier && event.key === Qt.Key_J)
    }
    function hideUnlessPinned() {
        if (!runnerWindow.pinned) {
            runnerWindow.visible = false
        }
    }

    implicitWidth: panelWidth + 2 * shadowPad
    implicitHeight: panel.height + 2 * shadowPad

    LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
    LayoutMirroring.childrenInherit: true

    Binding {
        target: root.runnerWindow
        property: "inputRect"
        value: Qt.rect(panel.x, panel.y, panel.width, panel.height)
    }

    Connections {
        target: root.runnerWindow
        function onHistoryBehaviorChanged() {
            root.runnerManager.historyEnabled = root.runnerWindow.historyBehavior !== RunnerWindow.Disabled
        }
        function onActivityChanged(activity) {
            if (activity) {
                root.runnerManager.setHistoryEnvironmentIdentifier(activity)
            }
        }
        function onVisibleChanged() {
            if (root.runnerWindow.visible) {
                apps.reset()
                opening.restart()
                queryField.forceActiveFocus()
                listView.currentIndex = -1
                if (root.runnerWindow.retainPriorSearch && !root.query) {
                    // A query passed over D-Bus wins over the prior search.
                    queryField.text = root.priorSearch
                    queryField.select(root.query.length, 0)
                    fadedTextCompletion.text = ""
                }
            } else {
                if (root.runnerWindow.retainPriorSearch) {
                    root.priorSearch = root.query
                }
                root.singleRunner = ""
                root.query = ""
                fadedTextCompletion.text = ""
                root.showHistory = false
            }
        }
    }
    Component.onCompleted: {
        runnerManager.historyEnabled = runnerWindow.historyBehavior !== RunnerWindow.Disabled
    }

    onQueryChanged: {
        queryField.text = query
        if (query.length > 0) {
            apps.current = null
        }
    }
    onShowHistoryChanged: {
        // 50 past searches are kept; 20 fit the panel.
        listView.model = showHistory ? runnerManager.history.slice(0, 20) : []
    }

    Loader {
        source: "QuickMenuBlur.qml"
    }

    // Spotlight's entrance (quickmenu.js openingMotion); Aurora Motion fades the window out when it closes.
    readonly property var motion: QuickMenu.openingMotion(Kirigami.Units.longDuration)
    ParallelAnimation {
        id: opening
        NumberAnimation { target: panel; property: "opacity"; from: 0; to: 1; duration: root.motion.fadeDuration; easing.type: Easing.OutCubic }
        NumberAnimation { target: panel; property: "scale"; from: root.motion.scaleFrom; to: 1; duration: root.motion.moveDuration; easing.type: Easing.OutQuint }
        NumberAnimation { target: panelLift; property: "y"; from: root.motion.liftFrom; to: 0; duration: root.motion.moveDuration; easing.type: Easing.OutQuint }
    }

    Item {
        id: panel

        x: root.shadowPad
        y: root.shadowPad
        transform: Translate { id: panelLift }
        width: root.panelWidth
        height: header.height + (body.visible ? divider.height + body.height : 0) + (rail.visible ? rail.height : 0)

        Keys.onEscapePressed: root.runnerWindow.visible = false

        QuickMenuGlass {
            id: glass
            anchors.fill: parent
            radius: Math.round(Kirigami.Units.gridUnit * 1.6)
        }

        RowLayout {
            id: header
            x: root.sidePadding
            width: parent.width - 2 * root.sidePadding
            height: root.headerHeight
            spacing: Kirigami.Units.largeSpacing

            Kirigami.Icon {
                Layout.preferredWidth: Math.round(root.headerHeight * 0.44)
                Layout.preferredHeight: Layout.preferredWidth
                source: "system-search-symbolic"
                color: Qt.rgba(1, 1, 1, 0.72)
                isMask: true
            }

            // Searching one runner only (krunner --runner, or a runner's own shortcut): its chip clears it.
            T.AbstractButton {
                visible: !!root.singleRunner
                implicitWidth: chip.implicitWidth + 2 * Kirigami.Units.mediumSpacing
                implicitHeight: Kirigami.Units.iconSizes.medium
                Accessible.description: i18n("Showing only results from %1", results.singleRunnerMetaData.name)
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: Accessible.description
                background: Rectangle {
                    radius: height / 2
                    color: Kirigami.Theme.highlightColor
                }
                contentItem: RowLayout {
                    id: chip
                    spacing: Kirigami.Units.smallSpacing
                    Kirigami.Icon {
                        Layout.preferredWidth: Kirigami.Units.iconSizes.small
                        Layout.preferredHeight: Kirigami.Units.iconSizes.small
                        source: results.singleRunnerMetaData.iconName
                    }
                    QQC2.Label {
                        text: results.singleRunnerMetaData.name
                        color: Kirigami.Theme.highlightedTextColor
                        textFormat: Text.PlainText
                    }
                }
                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }
                onClicked: {
                    root.singleRunner = ""
                    root.query = ""
                    fadedTextCompletion.text = ""
                    queryField.forceActiveFocus()
                }
            }

            QQC2.TextField {
                id: queryField

                property bool allowCompletion: false
                readonly property ListView activeListView: root.mode === "history" ? listView : results

                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                background: null
                leftPadding: 0
                rightPadding: 0
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.85
                color: Qt.rgba(1, 1, 1, 0.96)
                placeholderTextColor: Qt.rgba(1, 1, 1, 0.42)
                selectionColor: Kirigami.Theme.highlightColor
                selectedTextColor: Kirigami.Theme.highlightedTextColor
                activeFocusOnPress: true
                placeholderText: results.singleRunner ? i18nc("Textfield placeholder text, query specific KRunner plugin",
                                                              "Search '%1'…", results.singleRunnerMetaData.name)
                                                      : i18nc("Textfield placeholder text", "Quick Menu Search")

                // The rest of a past search, faded behind what is typed (KRunner's completion suggestion).
                QQC2.Label {
                    id: fadedTextCompletion
                    leftPadding: parent.leftPadding
                    topPadding: parent.topPadding
                    rightPadding: parent.rightPadding
                    bottomPadding: parent.bottomPadding
                    width: parent.width
                    height: parent.height
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                    visible: text !== queryField.text
                    opacity: 0.4
                    text: ""
                    textFormat: Text.PlainText
                    font: parent.font
                    color: parent.color
                    z: -1
                }

                function focusCurrentListView() {
                    if (listView.count > 0) {
                        listView.forceActiveFocus()
                    } else if (results.count > 0) {
                        results.forceActiveFocus()
                    }
                }

                onTextChanged: {
                    root.query = queryField.text
                    if (!allowCompletion || !root.query) {
                        fadedTextCompletion.text = ""
                    } else if (root.runnerWindow.historyBehavior === RunnerWindow.CompletionSuggestion) {
                        // Match the typed characters exactly, for case insensitive matches.
                        fadedTextCompletion.text = text + root.runnerManager.getHistorySuggestion(text).substring(text.length)
                    } else if (length > 0 && root.runnerWindow.historyBehavior === RunnerWindow.ImmediateCompletion) {
                        const oldText = text
                        const suggestedText = root.runnerManager.getHistorySuggestion(text)
                        if (suggestedText.length > 0) {
                            text = text + suggestedText.substr(oldText.length)
                            select(text.length, oldText.length)
                        }
                    }
                }

                Keys.onPressed: event => {
                    allowCompletion = (event.key !== Qt.Key_Backspace && event.key !== Qt.Key_Delete)
                    // The Applications view: the arrows walk the grid while the field keeps the typing.
                    if (root.mode === "apps" && apps.hasContent && event.modifiers === Qt.NoModifier) {
                        const key = event.key === Qt.Key_Down ? "down" : event.key === Qt.Key_Up ? "up"
                                  : event.key === Qt.Key_Left ? "left" : event.key === Qt.Key_Right ? "right" : ""
                        if (key && (apps.current || key === "down" || key === "up")) {
                            if (!apps.current && key === "up") {
                                if (root.runnerManager.historyEnabled && root.runnerManager.history.length > 0) {
                                    root.showHistory = true
                                    listView.currentIndex = 0
                                    focusCurrentListView()
                                }
                            } else {
                                apps.moveCurrent(key)
                            }
                            event.accepted = true
                            return
                        }
                    }
                    // Only End needs handling here; the first result is current by default.
                    if (event.key === Qt.Key_End && results.count > 0 && cursorPosition === text.length) {
                        results.currentIndex = results.count - 1
                        event.accepted = true
                        focusCurrentListView()
                    }
                    if (root.runnerWindow.historyBehavior === RunnerWindow.CompletionSuggestion
                            && fadedTextCompletion.text.length > 0 && cursorPosition === text.length && event.key === Qt.Key_Right) {
                        queryField.text = fadedTextCompletion.text
                        fadedTextCompletion.text = ""
                        event.accepted = true
                    }
                    if (root.isKeyUp(event) || root.isKeyDown(event)) {
                        if (root.isKeyUp(event)) {
                            activeListView.currentIndex = activeListView.count - 1
                        } else {
                            activeListView.incrementCurrentIndex()
                        }
                        if (queryField.text.length === 0) {
                            event.accepted = true
                            root.showHistory = true
                            focusCurrentListView()
                        } else {
                            return // KeyNavigation takes it to the list
                        }
                    }
                    !event.accepted && results.navigationKeyHandler(event)
                }
                Keys.onTabPressed: event => {
                    if (root.runnerWindow.historyBehavior === RunnerWindow.CompletionSuggestion
                            && fadedTextCompletion.text && queryField.text !== fadedTextCompletion.text) {
                        queryField.text = fadedTextCompletion.text
                        fadedTextCompletion.text = ""
                    } else {
                        event.accepted = false
                    }
                }
                function closeOrRun(event) {
                    if (root.mode === "apps" && apps.current) {
                        apps.launchCurrent()
                    } else if (!root.query) {
                        // Return in the empty field closes Quick Menu (KRunner's FEATURE 211225).
                        root.runnerWindow.visible = false
                    } else {
                        results.runCurrentIndex(event)
                    }
                }
                Keys.onEnterPressed: event => closeOrRun(event)
                Keys.onReturnPressed: event => closeOrRun(event)

                KeyNavigation.up: activeListView
                KeyNavigation.down: activeListView
            }

            PlasmaComponents3.BusyIndicator {
                Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                Layout.preferredHeight: Layout.preferredWidth
                running: queryTimer.queryDisplay
                visible: running

                Timer {
                    id: queryTimer
                    property bool queryDisplay: false
                    running: results.querying
                    repeat: true
                    interval: 500
                    onRunningChanged: if (queryDisplay && !running) {
                        queryDisplay = false
                    }
                    onTriggered: if (!queryDisplay) {
                        queryDisplay = true
                    }
                }
            }

            // Tahoe's ⋯: a ring with three dots, opening KRunner's own controls.
            T.AbstractButton {
                id: moreButton
                readonly property int dotSize: Math.max(2, Math.round(width / 9))
                implicitWidth: Math.round(root.headerHeight * 0.5)
                implicitHeight: implicitWidth
                Accessible.name: i18nc("@action:button Quick Menu options", "More")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: Accessible.name
                background: Rectangle {
                    radius: width / 2
                    color: Qt.rgba(1, 1, 1, moreButton.hovered || moreButton.down ? 0.14 : 0)
                    border.width: 1.5
                    border.color: Qt.rgba(1, 1, 1, 0.66)
                }
                contentItem: Item {
                    Row {
                        anchors.centerIn: parent
                        spacing: moreButton.dotSize
                        Repeater {
                            model: 3
                            Rectangle {
                                width: moreButton.dotSize
                                height: width
                                radius: width / 2
                                color: Qt.rgba(1, 1, 1, 0.8)
                            }
                        }
                    }
                }
                HoverHandler {
                    cursorShape: Qt.PointingHandCursor
                }
                onClicked: moreMenu.openRelative()

                PlasmaExtras.Menu {
                    id: moreMenu
                    visualParent: moreButton
                    placement: PlasmaExtras.Menu.BottomPosedRightAlignedPopup

                    PlasmaExtras.MenuItem {
                        text: i18n("Keep Open")
                        icon: "window-pin"
                        checkable: true
                        checked: root.runnerWindow.pinned
                        onClicked: root.runnerWindow.pinned = !root.runnerWindow.pinned
                    }
                    PlasmaExtras.MenuItem {
                        text: i18n("Show Usage Help")
                        icon: "question"
                        visible: root.runnerWindow.helpEnabled
                        onClicked: {
                            root.query = "?"
                            queryField.forceActiveFocus()
                        }
                    }
                    PlasmaExtras.MenuItem {
                        separator: true
                    }
                    PlasmaExtras.MenuItem {
                        text: i18n("Configure KRunner…")
                        icon: "configure"
                        visible: KAuthorized.authorizeControlModule("kcm_krunnersettings")
                        onClicked: {
                            root.runnerWindow.visible = false
                            KCMLauncher.open("plasma/kcms/desktop/kcm_krunnersettings")
                        }
                    }
                }
            }
        }

        Rectangle {
            id: divider
            anchors.top: header.bottom
            x: root.sidePadding
            width: parent.width - 2 * root.sidePadding
            height: 1
            color: Qt.rgba(1, 1, 1, 0.14)
            visible: body.visible
        }

        Item {
            id: body
            anchors.top: divider.bottom
            width: parent.width
            clip: true
            visible: root.mode === "apps" ? apps.hasContent : (root.mode === "history" ? listView.count > 0 : results.count > 0)
            height: Math.min(root.bodyMaxHeight, root.mode === "apps" ? apps.contentHeight
                                                : root.mode === "history" ? listView.contentHeight + listView.topMargin + listView.bottomMargin
                                                : results.contentHeight + results.topMargin + results.bottomMargin)

            // Aurora: the Menu key or Shift+F10 opens the menu of the app the arrows stand on, as on Windows. A
            // Shortcut takes the key before the search field does: the field's style opens its own text menu for it.
            Shortcut {
                sequences: ["Menu", "Shift+F10"]
                enabled: root.mode === "apps" && apps.current !== null
                onActivated: apps.openMenuForCurrent()
            }

            QuickMenuApps {
                id: apps
                anchors.fill: parent
                visible: root.mode === "apps"
                columns: root.columns
                iconSize: root.iconSize
                tileWidth: root.tileWidth
                sidePadding: root.sidePadding
                onLaunched: root.hideUnlessPinned()
            }

            Milou.ResultsView {
                    id: results
                    anchors.fill: parent
                    visible: root.mode === "results"
                    enabled: visible
                    clip: true
                    queryString: root.query
                    queryField: queryField
                    singleRunner: root.singleRunner
                    keyNavigationWraps: false
                    // The rows' icons line up with the search glass; the selection keeps a small inset.
                    topMargin: Kirigami.Units.smallSpacing
                    bottomMargin: Kirigami.Units.smallSpacing
                    leftMargin: root.sidePadding - Kirigami.Units.mediumSpacing
                    rightMargin: root.sidePadding - Kirigami.Units.mediumSpacing
                    boundsBehavior: Flickable.StopAtBounds
                    QQC2.ScrollBar.vertical: QuickMenuScrollBar {}
                    highlight: null
                    delegate: QuickMenuResult {}
                    section.delegate: QQC2.Label {
                        required property string section
                        width: results.width - results.leftMargin - results.rightMargin
                        topPadding: Kirigami.Units.mediumSpacing
                        bottomPadding: Kirigami.Units.smallSpacing
                        leftPadding: Kirigami.Units.mediumSpacing
                        text: section
                        font.weight: Font.DemiBold
                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                        color: Qt.rgba(1, 1, 1, 0.55)
                        textFormat: Text.PlainText
                    }

                    Keys.onEscapePressed: root.runnerWindow.visible = false
                    KeyNavigation.up: queryField
                    KeyNavigation.down: queryField

                    onActivated: root.hideUnlessPinned()
                    onUpdateQueryString: (text, cursorPosition) => {
                        queryField.text = text
                        queryField.select(cursorPosition, root.query.length)
                        queryField.focus = true
                    }
            }

            // Past searches, as Up recalls them on macOS: Return puts one back in the field, × forgets it.
            ListView {
                id: listView
                anchors.fill: parent
                visible: root.mode === "history"
                enabled: visible
                clip: true
                topMargin: Kirigami.Units.smallSpacing
                bottomMargin: Kirigami.Units.smallSpacing
                leftMargin: root.sidePadding - Kirigami.Units.mediumSpacing
                rightMargin: root.sidePadding - Kirigami.Units.mediumSpacing
                boundsBehavior: Flickable.StopAtBounds
                QQC2.ScrollBar.vertical: QuickMenuScrollBar {}
                keyNavigationWraps: false
                activeFocusOnTab: true
                highlightMoveDuration: 0
                model: []
                reuseItems: true

                delegate: T.ItemDelegate {
                    id: pastSearch
                    required property var modelData
                    required property int index
                    readonly property bool isCurrent: ListView.isCurrentItem
                    width: ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin
                    implicitHeight: Kirigami.Units.iconSizes.medium + 2 * Kirigami.Units.smallSpacing
                    hoverEnabled: true
                    leftPadding: Kirigami.Units.mediumSpacing
                    rightPadding: Kirigami.Units.mediumSpacing
                    Accessible.description: i18n("Recent Queries")
                    background: Rectangle {
                        radius: Kirigami.Units.cornerRadius * 2
                        color: pastSearch.isCurrent ? Kirigami.Theme.highlightColor : "transparent"
                    }
                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing
                        Kirigami.Icon {
                            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                            Layout.preferredHeight: Layout.preferredWidth
                            source: "view-history-symbolic"
                            color: pastSearch.isCurrent ? Kirigami.Theme.highlightedTextColor : Qt.rgba(1, 1, 1, 0.7)
                            isMask: true
                        }
                        QQC2.Label {
                            Layout.fillWidth: true
                            text: pastSearch.modelData
                            color: pastSearch.isCurrent ? Kirigami.Theme.highlightedTextColor : Qt.rgba(1, 1, 1, 0.94)
                            elide: Text.ElideRight
                            textFormat: Text.PlainText
                        }
                        T.AbstractButton {
                            visible: pastSearch.isCurrent
                            implicitWidth: Kirigami.Units.iconSizes.smallMedium
                            implicitHeight: implicitWidth
                            Accessible.name: i18n("Remove")
                            QQC2.ToolTip.visible: hovered
                            QQC2.ToolTip.text: Accessible.name
                            contentItem: Kirigami.Icon {
                                source: "edit-delete-remove-symbolic"
                                color: Kirigami.Theme.highlightedTextColor
                                isMask: true
                            }
                            HoverHandler {
                                cursorShape: Qt.PointingHandCursor
                            }
                            onClicked: {
                                listView.currentIndex = pastSearch.index
                                listView.runAction(0)
                            }
                        }
                    }
                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }
                    onHoveredChanged: if (hovered) listView.currentIndex = index
                    onClicked: {
                        listView.currentIndex = index
                        listView.runCurrentIndex(null)
                    }
                }

                onActiveFocusChanged: {
                    if (!activeFocus && currentIndex == listView.count - 1) {
                        currentIndex = 0
                    }
                }
                KeyNavigation.up: queryField
                KeyNavigation.down: queryField
                Keys.onReturnPressed: event => runCurrentIndex(event)
                Keys.onEnterPressed: event => runCurrentIndex(event)
                Keys.onTabPressed: {
                    if (currentIndex == listView.count - 1) {
                        listView.nextItemInFocusChain(true).forceActiveFocus()
                    } else {
                        incrementCurrentIndex()
                    }
                }
                Keys.onBacktabPressed: {
                    if (currentIndex == 0) {
                        listView.nextItemInFocusChain(false).forceActiveFocus()
                    } else {
                        decrementCurrentIndex()
                    }
                }
                Keys.onPressed: event => {
                    if (root.isKeyUp(event)) {
                        if (currentIndex !== 0) {
                            event.accepted = true
                        }
                        decrementCurrentIndex()
                    } else if (root.isKeyDown(event)) {
                        if (currentIndex !== count - 1) {
                            event.accepted = true
                        }
                        incrementCurrentIndex()
                    } else if (event.key === Qt.Key_Delete && event.modifiers === Qt.ShiftModifier) {
                        runAction(0)
                        event.accepted = true
                    } else if (event.text !== "" && !event.accepted) {
                        if (event.key == Qt.Key_Escape) {
                            root.showHistory = false
                        } else if (!/[\x00-\x1F\x7F]/.test(event.text)) {
                            // No unprintable control characters in the field.
                            queryField.text += event.text
                        }
                        queryField.focus = true
                    }
                }

                function runCurrentIndex(event) {
                    const entry = root.runnerManager.history[currentIndex]
                    if (!entry) {
                        return
                    }
                    // Shift+Return runs the first action, Remove.
                    if (event && event.modifiers === Qt.ShiftModifier) {
                        runAction(0)
                        return
                    }
                    queryField.text = entry
                    queryField.forceActiveFocus()
                }

                function runAction(actionIndex) {
                    if (actionIndex === 0) {
                        // QStringList changes reset the model: keep the index across it.
                        const index = listView.currentIndex
                        root.runnerManager.removeFromHistory(index)
                        model = root.runnerManager.history.slice(0, 20)
                        listView.currentIndex = Math.min(index, count - 1)
                    }
                }
            }
        }

        // Aurora: the account and power rail under the Applications view, as in Windows' Start.
        QuickMenuRail {
            id: rail
            anchors.top: body.visible ? body.bottom : divider.bottom
            width: parent.width
            visible: root.mode === "apps"
            menuParent: panel
            onLeaving: root.runnerWindow.visible = false
        }
    }
}
