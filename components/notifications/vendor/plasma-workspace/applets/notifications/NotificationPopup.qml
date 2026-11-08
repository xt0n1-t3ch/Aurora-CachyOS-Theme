/*
    SPDX-FileCopyrightText: 2019 Kai Uwe Broulik <kde@privat.broulik.de>

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

import QtQuick
import QtQuick.Layouts

import org.kde.kquickcontrolsaddons as KQuickAddons
import org.kde.kirigami as Kirigami

import org.kde.notificationmanager as NotificationManager
import plasma.applet.org.kde.plasma.notifications as NotificationsApplet

import "components" as Components
import "delegates" as Delegates

NotificationsApplet.NotificationWindow {
    id: notificationPopup

    property int popupWidth
    property bool showPopupTimeout

    // Aurora: a macOS banner of popupWidth, on glass, inside a transparent margin that holds its shadow and
    // the close button that overhangs its top-left corner on hover. The window takes input only over the banner.
    readonly property int bannerMargin: Kirigami.Units.gridUnit * 0.8
    readonly property int bannerRadius: Kirigami.Units.gridUnit * 1.2
    readonly property int bannerPaddingH: Kirigami.Units.gridUnit * 0.75
    readonly property int bannerPaddingV: Kirigami.Units.gridUnit * 0.65
    readonly property bool showControls: focusListener.containsMouse
    inputRect: Qt.rect(bannerMargin - closeButton.overhang, bannerMargin - closeButton.overhang,
                       popupWidth + 2 * closeButton.overhang, height - 2 * bannerMargin + 2 * closeButton.overhang)

    // Maximum width the popup can take to not break out of the screen geometry.
    readonly property int availableWidth: NotificationsApplet.Globals.screenRect.width - NotificationsApplet.Globals.popupEdgeDistance * 2 - leftPadding - rightPadding

    readonly property int minimumContentWidth: popupWidth
    readonly property int maximumContentWidth: Math.min((availableWidth > 0 ? availableWidth : Number.MAX_VALUE), popupWidth * 3)

    property alias modelInterface: notificationItem.modelInterface

    property int modelTimeout
    property int dismissTimeout

    property var defaultActionFallbackWindowIdx

    signal expired
    signal hoverEntered
    signal hoverExited

    property int defaultTimeout: 5000
    readonly property int effectiveTimeout: {
        if (modelTimeout === -1) {
            return defaultTimeout;
        }
        if (dismissTimeout) {
            return dismissTimeout;
        }
        return modelTimeout;
    }

    // On wayland we need focus to copy to the clipboard, we change on mouse interaction until the cursor leaves
    takeFocus: notificationItem.modelInterface.replying || focusListener.wantsFocus

    visible: false

    height: mainItem.implicitHeight + topPadding + bottomPadding
    width: mainItem.implicitWidth + leftPadding + rightPadding

    mainItem: KQuickAddons.MouseEventListener {
        id: focusListener
        property bool wantsFocus: false

        implicitWidth: notificationPopup.popupWidth + 2 * notificationPopup.bannerMargin
        implicitHeight: notificationItem.implicitHeight + 2 * (notificationPopup.bannerPaddingV + notificationPopup.bannerMargin)

        Components.BannerGlass {
            id: banner
            x: notificationPopup.bannerMargin
            y: notificationPopup.bannerMargin
            width: notificationPopup.popupWidth
            height: parent.height - 2 * notificationPopup.bannerMargin
            radius: notificationPopup.bannerRadius
        }

        Components.BannerBlur {}

        acceptedButtons: Qt.AllButtons
        hoverEnabled: true
        onPressed: wantsFocus = true
        onContainsMouseChanged: {
            wantsFocus = wantsFocus && containsMouse
            if (containsMouse) {
                onEntered: notificationPopup.hoverEntered()
            } else {
                onExited: notificationPopup.hoverExited()
            }
        }

        // Activate default action when dragging a file over the notification.
        DropArea {
            id: activateDefaultActionDropArea
            anchors.fill: parent

            property bool containsAcceptableDrag: false
            property point lastPosition: Qt.point(-1, -1)

            onEntered: (event) => {
                if (notificationItem.modelInterface.hasDefaultAction && !notificationItem.dragging) {
                    dragActivationTimer.restart();
                    containsAcceptableDrag = true;
                    lastPosition = Qt.point(drag.x, drag.y);
                } else {
                    drag.accepted = false;
                }
            }
            onPositionChanged: {
                if (containsAcceptableDrag) {
                    const manhattanLength = Math.abs((drag.x - lastPosition.x) + (drag.y - lastPosition.y));
                    if (manhattanLength > Application.styleHints.startDragDistance) {
                        dragActivationTimer.restart();
                        lastPosition = Qt.point(drag.x, drag.y);
                    }
                }
            }
            onDropped: {
                containsAcceptableDrag = false;
            }
            onExited: {
                containsAcceptableDrag = false;
                dragActivationTimer.stop();
            }
        }

        Timer {
            id: dragActivationTimer
            interval: 250 // same as Task Manager
            repeat: false
            onTriggered: notificationItem.modelInterface.defaultActionInvoked()
        }

        DraggableDelegate {
            anchors.fill: banner
            leftPadding: notificationPopup.bannerPaddingH
            rightPadding: notificationPopup.bannerPaddingH
            topPadding: notificationPopup.bannerPaddingV
            bottomPadding: notificationPopup.bannerPaddingV
            hoverEnabled: true
            draggable: notificationItem.modelInterface.notificationType != NotificationManager.Notifications.JobType
            onDismissRequested: NotificationsApplet.Globals.popupNotificationsModel.close(NotificationsApplet.Globals.popupNotificationsModel.index(notificationItem.modelInterface.index, 0))

            TapHandler {
                id: tapHandler
                acceptedButtons: {
                    let buttons = Qt.MiddleButton;
                    if (notificationPopup.modelInterface.hasDefaultAction) {
                        buttons |= Qt.LeftButton;
                    }
                    return buttons;
                }
                onTapped: (_eventPoint, button) => {
                    if (button === Qt.MiddleButton) {
                        if (notificationItem.modelInterface.closable) {
                            notificationItem.modelInterface.closeClicked();
                        }
                    } else if (notificationPopup.modelInterface.hasDefaultAction) {
                        notificationItem.modelInterface.defaultActionInvoked();
                    }
                }
            }

            LayoutMirroring.enabled: Application.layoutDirection === Qt.RightToLeft
            LayoutMirroring.childrenInherit: true

            Timer {
                id: timer
                interval: notificationPopup.effectiveTimeout
                running: {
                    if (!notificationPopup.visible) {
                        return false;
                    }
                    if (focusListener.containsMouse) {
                        return false;
                    }
                    if (interval <= 0) {
                        return false;
                    }
                    if (notificationItem.dragging || notificationItem.menuOpen || activateDefaultActionDropArea.containsAcceptableDrag) {
                        return false;
                    }
                    if (notificationItem.modelInterface.replying
                            && (notificationPopup.active || notificationItem.modelInterface.hasPendingReply)) {
                        return false;
                    }
                    return true;
                }
                onTriggered: {
                    if (notificationPopup.dismissTimeout) {
                        notificationPopup.modelInterface.dismissClicked();
                    } else {
                        notificationPopup.expired();
                    }
                }
            }

            NumberAnimation {
                target: notificationItem.modelInterface
                property: "remainingTime"
                from: timer.interval
                to: 0
                duration: timer.interval
                running: timer.running && Kirigami.Units.longDuration > 1 && notificationPopup.showPopupTimeout
            }

            contentItem: Delegates.DelegatePopup {
                id: notificationItem

                Layout.preferredHeight: implicitHeight // Why is this necessary?

                modelInterface {
                    maximumLineCount: 8
                    bodyCursorShape: modelInterface.hasDefaultAction ? Qt.PointingHandCursor : 0

                    popupLeftPadding: notificationPopup.bannerPaddingH
                    popupTopPadding: notificationPopup.bannerPaddingV
                    popupRightPadding: notificationPopup.bannerPaddingH
                    popupBottomPadding: notificationPopup.bannerPaddingV

                    // When notification is updated, restart hide timer
                    onTimeChanged: {
                        if (timer.running) {
                            timer.restart();
                        }
                    }
                    timeout: timer.running ? timer.interval : 0

                    closable: true

                    onBodyClicked: {
                        if (modelInterface.hasDefaultAction) {
                            notificationItem.modelInterface.defaultActionInvoked();
                        }
                    }
                }
            }
        }

        // macOS shows the close button on hover, overhanging the banner's top-left corner.
        Components.BannerButton {
            id: closeButton
            x: banner.x - overhang
            y: banner.y - overhang
            visible: notificationPopup.modelInterface.closable
            shown: notificationPopup.showControls
            iconName: "window-close-symbolic"
            text: i18nd("plasma_applet_org.kde.plasma.notifications", "Close")
            onClicked: notificationPopup.modelInterface.closeClicked()
        }

        // Plasma's per-app settings stay one click away, at the opposite corner.
        Components.BannerButton {
            x: banner.x + banner.width - width + overhang
            y: banner.y - overhang
            visible: notificationPopup.modelInterface.configurable
            shown: notificationPopup.showControls
            iconName: "configure-symbolic"
            text: notificationPopup.modelInterface.configureActionLabel || i18nd("plasma_applet_org.kde.plasma.notifications", "Configure")
            onClicked: notificationPopup.modelInterface.configureClicked()
        }
    }
}