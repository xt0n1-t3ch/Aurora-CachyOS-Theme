/*
    SPDX-FileCopyrightText: 2026 xt0n1-t3ch
    SPDX-License-Identifier: GPL-2.0-or-later
*/
// macOS's overlay scroll bar: a slim rounded thumb over the content, shown while scrolling or under the pointer and
// faded out otherwise.
import QtQuick
import QtQuick.Templates as T
import org.kde.kirigami as Kirigami

T.ScrollBar {
    id: bar

    implicitWidth: Kirigami.Units.smallSpacing * 2 + padding * 2
    padding: Kirigami.Units.smallSpacing / 2
    minimumSize: 0.1
    visible: size < 1

    contentItem: Rectangle {
        implicitWidth: Kirigami.Units.smallSpacing * (bar.hovered || bar.pressed ? 2 : 1.4)
        radius: width / 2
        color: Qt.rgba(1, 1, 1, bar.pressed ? 0.55 : 0.38)
        opacity: bar.active || bar.hovered ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: Kirigami.Units.longDuration }
        }
    }
}
