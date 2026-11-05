// SPDX-License-Identifier: GPL-3.0-only
// The menu bar icon. Like macOS, it opens the Control Center on press and wears a capsule while it is open.
import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.plasma.plasmoid

MouseArea {
    id: compact

    readonly property int iconSize: Plasmoid.configuration.mainIconHeight > 0 ? Plasmoid.configuration.mainIconHeight : 16

    Layout.minimumWidth: iconSize + 16
    Layout.preferredWidth: iconSize + 16
    hoverEnabled: true
    acceptedButtons: Qt.LeftButton
    onPressed: root.toggleControlCenter()

    Rectangle {
        anchors.centerIn: parent
        width: compact.iconSize + 16
        height: Math.min(compact.height - 6, 24)
        radius: height / 2
        color: "white"
        opacity: root.controlCenterOpen ? 0.20 : compact.containsMouse ? 0.10 : 0

        Behavior on opacity {
            NumberAnimation { duration: 120 }
        }
    }

    Kirigami.Icon {
        anchors.centerIn: parent
        width: compact.iconSize
        height: compact.iconSize
        source: Plasmoid.configuration.mainIconName
    }
}
