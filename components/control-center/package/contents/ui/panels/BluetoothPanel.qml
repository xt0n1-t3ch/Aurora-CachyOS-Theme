// SPDX-License-Identifier: GPL-3.0-only
// The paired Bluetooth devices: a click connects or disconnects one.
import QtQuick
import org.kde.bluezqt as BluezQt
import org.kde.kcmutils
import "../lib"

DetailPanel {
    id: panel

    property Item controlCenter
    readonly property QtObject manager: BluezQt.Manager
    readonly property var paired: {
        const list = []
        for (let i = 0; i < manager.devices.length; ++i) {
            if (manager.devices[i].paired) {
                list.push(manager.devices[i])
            }
        }
        return list
    }

    function glyphFor(type) {
        switch (type) {
        case BluezQt.Device.Headset:
        case BluezQt.Device.Headphones:
        case BluezQt.Device.AudioVideo:
            return "headphones"
        case BluezQt.Device.Keyboard:
            return "keyboard"
        case BluezQt.Device.Mouse:
            return "mouse"
        case BluezQt.Device.Phone:
            return "phone"
        default:
            return "device"
        }
    }

    title: "Bluetooth"
    hasSwitch: manager.adapters.length > 0
    switchOn: manager.bluetoothOperational
    onSwitchToggled: {
        const enable = !manager.bluetoothOperational
        manager.bluetoothBlocked = !enable
        for (let i = 0; i < manager.adapters.length; ++i) {
            manager.adapters[i].powered = enable
        }
    }
    section: "My Devices"
    emptyText: manager.bluetoothOperational ? "No paired devices" : "Bluetooth is off"
    settingsText: "Bluetooth Settings…"
    onSettingsClicked: {
        controlCenter.close()
        KCMLauncher.openSystemSettings("kcm_bluetooth")
    }

    // Nothing is listed while Bluetooth is off.
    model: manager.bluetoothOperational ? paired : []

    delegate: ListRow {
        required property var modelData

        glyph: panel.glyphFor(modelData.type)
        title: modelData.name
        on: modelData.connected
        note: modelData.connected ? "Connected" : ""
        onClicked: modelData.connected ? modelData.disconnectFromDevice() : modelData.connectToDevice()
    }
}
