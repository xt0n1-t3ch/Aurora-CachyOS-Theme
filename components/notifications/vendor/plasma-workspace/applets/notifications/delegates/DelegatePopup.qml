/*
    SPDX-FileCopyrightText: 2018-2019 Kai Uwe Broulik <kde@privat.broulik.de>
    SPDX-FileCopyrightText: 2024 Marco Martin <mart@kde.org>
    SPDX-FileCopyrightText: 2026 xt0n1-t3ch

    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

// Aurora: the popup laid out like a macOS banner. The app icon sits large at the leading edge, the title is
// bold with the time at the trailing end of its line, the body follows, and a notification's own image is a
// thumbnail at the trailing edge. The app name is the title only when the notification has none, as macOS does
// (Apple HIG, Notifications: "the system displays your app name if you don't provide a title").

import QtQuick
import QtQuick.Effects
import QtQuick.Layouts

import org.kde.plasma.core as PlasmaCore
import org.kde.plasma.components as PlasmaComponents3
import org.kde.kirigami as Kirigami
import org.kde.kquickcontrolsaddons as KQuickControlsAddons
import org.kde.coreaddons as KCoreAddons

import org.kde.notificationmanager as NotificationManager
import plasma.applet.org.kde.plasma.notifications as Notifications

import "../components" as Components

BaseDelegate {
    id: delegateRoot

    body: bodyLabel
    icon: thumbnail
    footer: footerLoader.item as Item
    columns: 3
    rowSpacing: 2
    columnSpacing: Kirigami.Units.gridUnit * 0.65

    Accessible.role: Accessible.Notification

    readonly property color primaryText: "#ffffff"
    // A long body shows its first lines. In the popover's history it is `expandable`: a click on it, or on Show More,
    // shows all of it, and Show Less folds it back.
    property real maximumBodyLines: 4
    property bool expandable: false
    property bool bodyExpanded: false
    // True while the popover shows Clear over this notification's top-right corner, where the time sits; the time
    // fades out under it.
    property bool clearShown: false
    readonly property real bodyLines: expandable && bodyExpanded ? Infinity : maximumBodyLines
    readonly property bool bodyFolds: expandable && bodyLabel.implicitHeight > bodyMetrics.lineSpacing * maximumBodyLines + 1
    readonly property color secondaryText: Qt.rgba(1, 1, 1, 0.55)

    // The app's icon, or the notification's own when the app has none.
    Kirigami.Icon {
        id: appIcon
        Layout.row: 0
        Layout.column: 0
        Layout.rowSpan: 2
        Layout.alignment: Qt.AlignVCenter
        implicitWidth: Kirigami.Units.iconSizes.medium
        implicitHeight: Kirigami.Units.iconSizes.medium
        source: delegateRoot.modelInterface.applicationIconSource || delegateRoot.modelInterface.icon || "preferences-desktop-notification"
        smooth: true
    }

    // Plasma's title logic (job states included), read by the visible title below.
    Components.Summary {
        id: summary
        visible: false
        modelInterface: delegateRoot.modelInterface
    }

    RowLayout {
        Layout.row: 0
        Layout.column: 1
        // Without a thumbnail the title line runs to the banner's edge, so the time sits at the trailing corner.
        Layout.columnSpan: thumbnail.visible ? 1 : 2
        Layout.fillWidth: true
        // A thumbnail taller than the text gives its height to both rows; the text stays together in the middle.
        Layout.alignment: Qt.AlignBottom
        spacing: Kirigami.Units.smallSpacing

        PlasmaComponents3.Label {
            id: titleLabel
            Layout.fillWidth: true
            text: summary.text || delegateRoot.modelInterface.applicationName
            textFormat: Text.PlainText
            font.weight: Font.DemiBold
            color: delegateRoot.primaryText
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        PlasmaComponents3.Label {
            id: ageLabel
            property string ageText: ""
            text: ageText
            visible: text !== ""
            opacity: delegateRoot.clearShown ? 0 : 1
            Behavior on opacity {
                NumberAnimation { duration: Kirigami.Units.shortDuration }
            }
            font: Kirigami.Theme.smallFont
            color: delegateRoot.secondaryText

            function update(): void {
                const time = delegateRoot.modelInterface.time;
                const job = delegateRoot.modelInterface.jobState;
                if (!time || isNaN(time.getTime()) || job === NotificationManager.Notifications.JobStateRunning
                        || job === NotificationManager.Notifications.JobStateSuspended) {
                    ageText = "";
                    return;
                }
                const minutes = Math.floor((Date.now() - time.getTime()) / 60000);
                if (minutes < 1) {
                    ageText = i18ndc("plasma_applet_org.kde.plasma.notifications", "Notification age, as macOS writes it", "now");
                } else if (minutes < 60) {
                    ageText = i18ndcp("plasma_applet_org.kde.plasma.notifications", "Notification was added minutes ago, keep short", "%1 min ago", "%1 min ago", minutes);
                } else if (minutes < 60 * 22) {
                    ageText = Qt.formatTime(time, Qt.locale().timeFormat(Locale.ShortFormat).replace(/.ss?/i, ""));
                } else {
                    // Older entries in the history read as a date ("Yesterday"), as Plasma's history row does.
                    ageText = KCoreAddons.Format.formatRelativeDate(time, Locale.ShortFormat);
                }
            }

            Component.onCompleted: update()
            Connections {
                target: delegateRoot.modelInterface
                function onTimeChanged(): void { ageLabel.update(); }
            }
            Connections {
                target: Notifications.Globals
                function onTimeChanged(): void { ageLabel.update(); }
            }

            PlasmaCore.ToolTipArea {
                anchors.fill: parent
                active: ageLabel.visible
                subText: delegateRoot.modelInterface.time ? delegateRoot.modelInterface.time.toLocaleString(Qt.locale(), Locale.LongFormat) : ""
            }
        }
    }

    KQuickControlsAddons.MouseEventListener {
        Layout.row: 1
        Layout.column: 1
        Layout.columnSpan: thumbnail.visible ? 1 : 2
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignTop
        Layout.maximumHeight: bodyMetrics.lineSpacing * delegateRoot.bodyLines
        implicitWidth: -1 // the body does not drive the width, which avoids a binding loop
        implicitHeight: Math.min(bodyLabel.implicitHeight, bodyMetrics.lineSpacing * delegateRoot.bodyLines)
        visible: delegateRoot.hasBodyText
        clip: true
        onClicked: {
            if (delegateRoot.bodyFolds && !delegateRoot.hoveredLink) {
                delegateRoot.bodyExpanded = !delegateRoot.bodyExpanded;
            } else if (delegateRoot.modelInterface.hasDefaultAction && !delegateRoot.hoveredLink) {
                delegateRoot.modelInterface.defaultActionInvoked();
            }
        }

        FontMetrics {
            id: bodyMetrics
            font: bodyLabel.font
        }

        Components.Body {
            id: bodyLabel
            width: parent.width
            opacity: 0.88
            modelInterface: delegateRoot.modelInterface
            Accessible.ignored: true // ignore HTML body in favor of Accessible.description on delegateRoot
        }
    }

    // A notification's own picture (a screenshot, an album cover), as a thumbnail at the trailing edge.
    Components.Icon {
        id: thumbnail
        Layout.row: 0
        Layout.column: 2
        Layout.rowSpan: 2
        Layout.alignment: Qt.AlignVCenter
        implicitWidth: Kirigami.Units.iconSizes.large
        implicitHeight: Kirigami.Units.iconSizes.large
        modelInterface: delegateRoot.modelInterface
        // Without an app icon the leading icon already shows the notification's own, so it is not shown twice.
        source: delegateRoot.modelInterface.applicationIconSource
                && delegateRoot.modelInterface.icon !== delegateRoot.modelInterface.applicationIconSource ? delegateRoot.modelInterface.icon : null
        // Rounded corners, as macOS draws a notification's picture.
        layer.enabled: visible
        layer.effect: MultiEffect {
            maskEnabled: true
            maskSource: thumbnailMask
            maskThresholdMin: 0.5
            maskSpreadAtMin: 1.0
        }
    }

    Rectangle {
        id: thumbnailMask
        width: thumbnail.width
        height: thumbnail.height
        radius: Kirigami.Units.cornerRadius
        visible: false
        layer.enabled: true
    }

    PlasmaComponents3.Label {
        Layout.row: 2
        Layout.column: 1
        Layout.columnSpan: 2
        Layout.fillWidth: true
        // Its text must not widen the column the body wraps in: a wider body folds less, which hides this label, which
        // narrows the body again, a layout loop.
        Layout.preferredWidth: 0
        Layout.minimumWidth: 0
        elide: Text.ElideRight
        visible: delegateRoot.bodyFolds
        text: delegateRoot.bodyExpanded ? i18nc("@action:button fold a long notification", "Show Less")
                                        : i18nc("@action:button unfold a long notification", "Show More")
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.linkColor
        Accessible.role: Accessible.Button

        TapHandler {
            onTapped: delegateRoot.bodyExpanded = !delegateRoot.bodyExpanded
        }
        HoverHandler {
            cursorShape: Qt.PointingHandCursor
        }
    }

    Components.FooterLoader {
        id: footerLoader
        Layout.row: 3
        Layout.column: 1
        Layout.columnSpan: 2
        Layout.fillWidth: true
        Layout.topMargin: item ? Kirigami.Units.smallSpacing : 0
        modelInterface: delegateRoot.modelInterface
        iconContainerItem: thumbnail
    }
}
