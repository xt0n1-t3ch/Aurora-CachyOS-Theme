/*
    SPDX-FileCopyrightText: 2013-2014 Vishesh Handa <me@vhanda.in>
    SPDX-FileCopyrightText: 2015-2016 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2026 xt0n1-t3ch
    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/
// A search result as Tahoe's Spotlight lists it: icon, name and a quieter second line, the selected row filled
// with the accent colour. It keeps Milou's ResultDelegate contract (actions, activeAction and the activate*Action
// functions that Milou.ResultsView calls on Tab), its click-to-run and its drag-out of files and links.
import QtQuick
import QtQuick.Layouts
import QtQuick.Templates as T
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami
import org.kde.milou as Milou

T.ItemDelegate {
    id: result

    required property int index
    required property var model
    required property var actions

    property int activeAction: -1
    readonly property bool isCurrent: ListView.isCurrentItem
    readonly property color ink: isCurrent ? Kirigami.Theme.highlightedTextColor : Qt.rgba(1, 1, 1, 0.94)

    width: ListView.view ? ListView.view.width - ListView.view.leftMargin - ListView.view.rightMargin : implicitWidth
    implicitHeight: Math.max(Kirigami.Units.iconSizes.medium, labels.implicitHeight) + 2 * Kirigami.Units.mediumSpacing
    hoverEnabled: true

    Accessible.role: Accessible.ListItem
    Accessible.name: title.text
    Accessible.description: String(model.subtext || "")

    onIsCurrentChanged: if (!isCurrent) activeAction = -1

    function activateNextAction() {
        if (activeAction === actionButtons.count - 1) {
            return false
        }
        ++activeAction
        return true
    }
    function activatePreviousAction() {
        if (activeAction < 0) {
            return false
        }
        --activeAction
        return true
    }
    function activateLastAction() {
        activeAction = actionButtons.count - 1
    }

    onClicked: {
        ListView.view.currentIndex = index
        ListView.view.runCurrentIndex()
    }
    onHoveredChanged: if (hovered) ListView.view.currentIndex = index

    background: Rectangle {
        radius: Kirigami.Units.cornerRadius * 2
        color: result.isCurrent ? Kirigami.Theme.highlightColor : "transparent"
    }

    HoverHandler {
        cursorShape: Qt.PointingHandCursor
    }

    Item {
        id: dragHelper
        Drag.dragType: Drag.Automatic
        Drag.supportedActions: Qt.CopyAction | Qt.LinkAction
        Drag.onDragFinished: {
            Drag.mimeData = {}
            Drag.imageSource = ""
        }
    }
    DragHandler {
        id: drag
        target: null
        grabPermissions: PointerHandler.TakeOverForbidden
        onActiveChanged: if (active) {
            icon.grabToImage(grab => {
                const view = result.ListView.view
                const mimeData = view.model.getMimeData(view.model.index(result.index, 0))
                if (!mimeData) {
                    return
                }
                dragHelper.Drag.imageSource = grab.url
                dragHelper.Drag.mimeData = Milou.MouseHelper.generateMimeDataMap(mimeData)
                dragHelper.Drag.active = drag.active
            })
        } else {
            dragHelper.Drag.active = false
        }
    }

    leftPadding: Kirigami.Units.mediumSpacing
    rightPadding: Kirigami.Units.mediumSpacing

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Icon {
            id: icon
            Layout.preferredWidth: Kirigami.Units.iconSizes.medium
            Layout.preferredHeight: Kirigami.Units.iconSizes.medium
            Layout.alignment: Qt.AlignVCenter
            source: result.model.decoration
            animated: false
        }

        ColumnLayout {
            id: labels
            Layout.fillWidth: true
            spacing: 0

            QQC2.Label {
                id: title
                Layout.fillWidth: true
                text: result.model.display
                color: result.ink
                elide: Text.ElideMiddle
                wrapMode: result.model.multiLine ? Text.WordWrap : Text.NoWrap
                maximumLineCount: result.model.multiLine ? 20 : 1
                // Multi-line answers (a calculation, a definition) come styled; one-liners are plain names.
                textFormat: result.model.multiLine ? Text.StyledText : Text.PlainText
            }
            QQC2.Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: String(result.model.subtext || "")
                color: result.ink
                opacity: 0.62
                font: Kirigami.Theme.smallFont
                elide: Text.ElideMiddle
                maximumLineCount: 1
                textFormat: Text.PlainText
            }
        }

        Row {
            Layout.alignment: Qt.AlignVCenter
            spacing: Kirigami.Units.smallSpacing
            visible: result.isCurrent && actionButtons.count > 0

            Repeater {
                id: actionButtons
                model: result.actions

                T.AbstractButton {
                    id: actionButton
                    required property var modelData
                    required property int index
                    width: Kirigami.Units.iconSizes.medium
                    height: width
                    Accessible.role: Accessible.Button
                    Accessible.name: modelData.text || ""
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: index === 0 ? i18ndc("milou", "placeholder is action e.g. run in terminal, in parenthesis is shortcut",
                                                            "%1 (Shift+Return)", modelData.text || "")
                                                   : (modelData.text || "")
                    background: Rectangle {
                        radius: width / 2
                        color: Qt.rgba(1, 1, 1, result.activeAction === actionButton.index || actionButton.hovered ? 0.28 : 0.14)
                    }
                    contentItem: Kirigami.Icon {
                        source: actionButton.modelData.iconSource || ""
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: Kirigami.Units.iconSizes.small
                        color: result.ink
                        isMask: true
                    }
                    HoverHandler {
                        cursorShape: Qt.PointingHandCursor
                    }
                    onClicked: result.ListView.view.runAction(index)
                }
            }
        }
    }
}
