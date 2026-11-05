// SPDX-License-Identifier: GPL-3.0-only
// The network list the Network pill opens: every connection Plasma knows, a click connects or disconnects.
import QtQuick
import org.kde.kcmutils
import org.kde.plasma.networkmanagement as PlasmaNM
import "../lib"

DetailPanel {
    id: panel

    property Item controlCenter

    title: devices.wirelessDeviceAvailable ? "Wi-Fi" : "Network"
    hasSwitch: devices.wirelessDeviceAvailable
    switchOn: enabledConnections.wirelessEnabled
    onSwitchToggled: handler.enableWireless(!enabledConnections.wirelessEnabled)
    section: "Connections"
    emptyText: "No connections"
    settingsText: "Network Settings…"
    onSettingsClicked: {
        controlCenter.close()
        KCMLauncher.openSystemSettings("kcm_networkmanagement")
    }

    model: PlasmaNM.AppletProxyModel {
        sourceModel: PlasmaNM.NetworkModel {}
    }

    delegate: ListRow {
        required property var model
        readonly property bool connected: model.ConnectionState === PlasmaNM.Enums.Activated

        glyph: model.Type === PlasmaNM.Enums.Wireless ? "wifi" : model.Type === PlasmaNM.Enums.Wired ? "ethernet" : "device"
        title: model.Name
        on: connected
        busy: model.ConnectionState === PlasmaNM.Enums.Activating || model.ConnectionState === PlasmaNM.Enums.Deactivating
        note: connected ? "Connected" : ""
        onClicked: {
            if (connected) {
                handler.deactivateConnection(model.ConnectionPath, model.DevicePath)
            } else if (!model.Uuid) {
                // New networks need credentials: KDE's connection editor asks for them.
                panel.controlCenter.close()
                KCMLauncher.openSystemSettings("kcm_networkmanagement")
            } else {
                handler.activateConnection(model.ConnectionPath, model.DevicePath, model.SpecificPath)
            }
        }
    }

    PlasmaNM.AvailableDevices { id: devices }
    PlasmaNM.EnabledConnections { id: enabledConnections }
    PlasmaNM.Handler { id: handler }
}
