/*
    SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
    SPDX-License-Identifier: GPL-2.0-or-later
*/
// The rail along the bottom of the Applications view, as in Windows 11's Start: the user's picture and name on the
// left open the account menu (Lock, Switch User, Log Out), the power button on the right opens the power menu
// (Sleep, Hibernate, Restart, Shut Down). Both menus are the Applications view's own menu (QuickMenuPopup.qml) and
// list only what the session allows. Plasma's session manager does the work, with its own confirmation.
import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import QtQuick.Templates as T

import org.kde.coreaddons as KCoreAddons
import org.kde.kirigami as Kirigami
import org.kde.plasma.private.sessions as Sessions

import "quickmenu.js" as QuickMenu

Item {
    id: rail

    // The item the menus open in: the whole panel, so they can rise above the rail.
    required property Item menuParent
    // Asked before an action that leaves the desktop, so the panel can close first.
    signal leaving()

    implicitHeight: Math.round(Kirigami.Units.gridUnit * 3.2)

    KCoreAddons.KUser {
        id: user
    }

    Sessions.SessionManagement {
        id: session
    }

    function run(action) {
        rail.leaving()
        QuickMenu.runSessionAction(session, action)
    }

    // Opens `list` above `button`, its left edge (or right edge, for the power menu) lined up with the button's.
    function openMenu(button, list, alignRight) {
        const corner = button.mapToItem(menuParent, alignRight ? button.width : 0, 0)
        const size = menu.measure(list)
        menu.openAt(null, list, alignRight ? corner.x - size.width : corner.x, corner.y - size.height - Kirigami.Units.smallSpacing, false)
    }

    QuickMenuPopup {
        id: menu
        parent: rail.menuParent
        onChosen: (entry, action) => {
            menu.close()
            rail.run(action)
        }
    }

    Rectangle {
        id: hairline
        x: Math.round(Kirigami.Units.gridUnit * 1.1)
        width: parent.width - 2 * x
        height: 1
        color: Qt.rgba(1, 1, 1, 0.14)
    }

    RowLayout {
        anchors {
            fill: parent
            topMargin: 1
            leftMargin: Math.round(Kirigami.Units.gridUnit * 0.7)
            rightMargin: Math.round(Kirigami.Units.gridUnit * 0.7)
        }
        spacing: Kirigami.Units.largeSpacing

        // The account: picture and name, one rounded hover area, as Windows draws it.
        T.AbstractButton {
            id: account
            Layout.alignment: Qt.AlignVCenter
            implicitHeight: Math.round(Kirigami.Units.gridUnit * 2.3)
            implicitWidth: accountRow.implicitWidth + 2 * Kirigami.Units.mediumSpacing
            Accessible.name: i18n("Account: %1", nameLabel.text)
            background: Rectangle {
                radius: height / 2
                color: Qt.rgba(1, 1, 1, account.down ? 0.16 : account.hovered ? 0.10 : 0)
                Behavior on color { ColorAnimation { duration: Kirigami.Units.shortDuration } }
            }
            contentItem: RowLayout {
                id: accountRow
                spacing: Kirigami.Units.mediumSpacing + Kirigami.Units.smallSpacing
                Item {
                    Layout.preferredWidth: Math.round(Kirigami.Units.gridUnit * 1.75)
                    Layout.preferredHeight: Layout.preferredWidth
                    Rectangle {
                        anchors.fill: parent
                        radius: width / 2
                        color: Kirigami.Theme.highlightColor
                        visible: face.status !== Image.Ready
                        Kirigami.Icon {
                            anchors.centerIn: parent
                            width: Math.round(parent.width * 0.62)
                            height: width
                            source: "user-symbolic"
                            color: "white"
                            isMask: true
                        }
                    }
                    Kirigami.ShadowedImage {
                        id: face
                        anchors.fill: parent
                        source: user.faceIconUrl
                        radius: width / 2
                        sourceSize: Qt.size(width * 2, height * 2)
                        fillMode: Image.PreserveAspectCrop
                        visible: status === Image.Ready
                    }
                }
                QQC2.Label {
                    id: nameLabel
                    text: user.fullName || user.loginName
                    color: Qt.rgba(1, 1, 1, 0.92)
                    font.weight: Font.Medium
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    Layout.maximumWidth: Kirigami.Units.gridUnit * 12
                }
            }
            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }
            onClicked: rail.openMenu(account, QuickMenu.accountItems(session), false)
        }

        Item {
            Layout.fillWidth: true
        }

        // The power button: a round glass button, as the Control Center's.
        T.AbstractButton {
            id: power
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: Math.round(Kirigami.Units.gridUnit * 2.3)
            implicitHeight: implicitWidth
            Accessible.name: i18n("Power")
            QQC2.ToolTip.visible: hovered && !menu.opened
            QQC2.ToolTip.text: Accessible.name
            QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
            background: Rectangle {
                radius: width / 2
                color: Qt.rgba(1, 1, 1, power.down ? 0.18 : power.hovered ? 0.12 : 0.06)
                border.width: 1
                border.color: Qt.rgba(1, 1, 1, 0.10)
                Behavior on color { ColorAnimation { duration: Kirigami.Units.shortDuration } }
            }
            contentItem: Item {
                Kirigami.Icon {
                    anchors.centerIn: parent
                    width: Kirigami.Units.iconSizes.smallMedium
                    height: width
                    source: "system-shutdown-symbolic"
                    color: Qt.rgba(1, 1, 1, 0.9)
                    isMask: true
                }
            }
            HoverHandler {
                cursorShape: Qt.PointingHandCursor
            }
            onClicked: rail.openMenu(power, QuickMenu.powerItems(session), true)
        }
    }
}
