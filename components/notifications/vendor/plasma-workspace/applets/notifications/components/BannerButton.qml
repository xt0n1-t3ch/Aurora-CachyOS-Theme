/*
    SPDX-FileCopyrightText: 2026 xt0n1-t3ch
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
// The small round glass button that a notification shows on hover, overhanging one of its corners as macOS does:
// Clear at the top-left, Plasma's per-app settings at the top-right. Banners and the popover's rows share it.
import QtQuick

import org.kde.kirigami as Kirigami

Rectangle {
    id: button

    // How far the button reaches past the corner it sits on.
    readonly property int overhang: Math.round(width / 3)
    property string iconName
    property string text
    // Whether the button is shown; it fades rather than pops, and takes no clicks while hidden.
    property bool shown
    signal clicked

    width: Kirigami.Units.iconSizes.small + Kirigami.Units.smallSpacing * 2
    height: width
    radius: width / 2
    color: buttonArea.containsMouse ? Qt.rgba(0.30, 0.30, 0.33, 0.96) : Qt.rgba(0.20, 0.20, 0.22, 0.94)
    border.color: Qt.rgba(1, 1, 1, 0.16)
    border.width: 1
    opacity: shown ? 1 : 0
    enabled: opacity > 0
    Behavior on opacity {
        NumberAnimation { duration: Kirigami.Units.shortDuration; easing.type: Easing.OutCubic }
    }
    Accessible.role: Accessible.Button
    Accessible.name: text

    Kirigami.Icon {
        anchors.centerIn: parent
        width: Math.round(parent.width * 0.55)
        height: width
        source: button.iconName
        color: "#ffffff"
        isMask: true
    }

    MouseArea {
        id: buttonArea
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: button.clicked()
    }
}
