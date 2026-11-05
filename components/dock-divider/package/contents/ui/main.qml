// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The divider widget of the Dock panel. It lies across a horizontal or a vertical panel.
import QtQuick
import QtQuick.Layouts
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore

// A one-pixel divider, like the line between the apps and the stacks in the macOS Dock.
PlasmoidItem {
    id: root

    readonly property bool vertical: Plasmoid.formFactor === PlasmaCore.Types.Vertical

    preferredRepresentation: fullRepresentation
    Plasmoid.backgroundHints: PlasmaCore.Types.NoBackground

    fullRepresentation: Item {
        // Fixed at 17 px along the panel: the 1 px line with 8 px of space on each side.
        Layout.minimumWidth: root.vertical ? -1 : 17
        Layout.maximumWidth: root.vertical ? -1 : 17
        Layout.minimumHeight: root.vertical ? 17 : -1
        Layout.maximumHeight: root.vertical ? 17 : -1

        // The line covers 62% of the panel's thickness.
        Rectangle {
            anchors.centerIn: parent
            width: root.vertical ? parent.width * 0.62 : 1
            height: root.vertical ? 1 : parent.height * 0.62
            color: Qt.rgba(1, 1, 1, 0.3)
        }
    }
}
