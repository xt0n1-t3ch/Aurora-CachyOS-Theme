// SPDX-License-Identifier: GPL-3.0-only
// The Aurora Control Center widget for Plasma 6: a menu bar icon that opens a macOS Tahoe style Control Center. Based
// on the Control Centre of Prayag Jain and Mkeays501 (GPL-3.0).
import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import org.kde.plasma.plasmoid
import org.kde.plasma.core as PlasmaCore

PlasmoidItem {
    id: root

    // Whether the widget sits in a panel, where it shows the menu bar icon. Elsewhere it shows the Control Center
    // itself.
    readonly property bool inPanel: Plasmoid.location === PlasmaCore.Types.TopEdge
        || Plasmoid.location === PlasmaCore.Types.RightEdge
        || Plasmoid.location === PlasmaCore.Types.BottomEdge
        || Plasmoid.location === PlasmaCore.Types.LeftEdge
    // Whether the Control Center window is open; the menu bar icon draws its capsule from it.
    readonly property alias controlCenterOpen: controlCenter.visible

    // Opens the Control Center window below the menu bar icon, or closes it when it is open.
    function toggleControlCenter() {
        controlCenter.anchorItem = root.compactRepresentationItem
        controlCenter.toggle()
    }

    // The Control Center is its own transparent window (ControlCenterWindow), not Plasma's popup, whose
    // background would put one sheet of glass behind all the modules. Libplasma still needs a full representation
    // (without one it shows the applet expanded and the icon disappears), so in a panel it is an empty item.
    preferredRepresentation: inPanel ? compactRepresentation : fullRepresentation
    compactRepresentation: CompactRepresentation {}

    // Aurora shows the hand on every clickable surface of the shell: the Dock, the bar, the tray, popups.
    // The Control Center is always in the bar, so it carries the one PointerHints for the whole plasmashell.
    Loader {
        source: Qt.resolvedUrl("lib/PointerHintsImpl.qml")
    }
    fullRepresentation: Item {
        id: full

        Layout.preferredWidth: desktop.item ? desktop.item.implicitWidth : 0
        Layout.preferredHeight: desktop.item ? desktop.item.implicitHeight : 0

        Loader {
            id: desktop
            active: !root.inPanel
            sourceComponent: ControlCenter {}
        }

        // Activating the widget (its global shortcut calls setExpanded(true), plasmoiditem.cpp:94 in libplasma 6.8.2)
        // shows Plasma's popup with this empty item. Hide that popup before it paints and open or close the window
        // instead. The hide replaces the popup's binding (CompactApplet.qml: expanded && fullRepresentation), so it is
        // put back once expanded is false again, ready for the next activation.
        Connections {
            target: root.inPanel ? full.Window.window : null
            function onVisibleChanged() {
                const popup = full.Window.window
                if (popup && popup.visible) {
                    popup.visible = false
                    root.expanded = false
                    popup.visible = Qt.binding(() => root.expanded && !!full)
                    root.toggleControlCenter()
                }
            }
        }
    }
    toolTipMainText: "Control Center"
    toolTipSubText: ""

    ControlCenterWindow {
        id: controlCenter
    }
}
