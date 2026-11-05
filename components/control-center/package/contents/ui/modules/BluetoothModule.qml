// SPDX-License-Identifier: GPL-3.0-only
// Bluetooth: the circle powers the adapters on or off; the pill opens the paired devices.
import QtQuick
import org.kde.bluezqt as BluezQt
import "../lib"
import "../js/cc.js" as CC

ModulePill {
    id: module

    property Item controlCenter
    readonly property QtObject manager: BluezQt.Manager
    readonly property var summary: CC.bluetoothSummary({
        adapters: manager.adapters.length,
        operational: manager.bluetoothOperational,
        connected: connectedNames()
    })

    function connectedNames() {
        const names = []
        for (let i = 0; i < manager.devices.length; ++i) {
            if (manager.devices[i].connected) {
                names.push(manager.devices[i].name)
            }
        }
        return names
    }

    // The same switch as Plasma's Bluetooth applet: unblock and power every adapter, or the reverse.
    function toggle() {
        const enable = !manager.bluetoothOperational
        manager.bluetoothBlocked = !enable
        for (let i = 0; i < manager.adapters.length; ++i) {
            manager.adapters[i].powered = enable
        }
    }

    glyph: "bluetooth"
    title: "Bluetooth"
    subtitle: summary.subtitle
    on: summary.active
    toggleEnabled: summary.available
    onToggled: toggle()
    onOpened: controlCenter.openPanel("bluetooth")
}
