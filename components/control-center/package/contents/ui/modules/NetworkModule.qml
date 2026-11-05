// SPDX-License-Identifier: GPL-3.0-only
// Network: Wi-Fi when there is a Wi-Fi device, Ethernet otherwise. The circle switches Wi-Fi; the pill opens the list.
import QtQuick
import org.kde.plasma.networkmanagement as PlasmaNM
import "../lib"
import "../js/cc.js" as CC

ModulePill {
    id: module

    property Item controlCenter
    readonly property var summary: CC.networkSummary({
        wifi: devices.wirelessDeviceAvailable,
        wifiOn: enabledConnections.wirelessEnabled,
        icon: connectionIcon.connectionIcon,
        status: networkStatus.activeConnections
    })

    title: summary.title
    subtitle: summary.subtitle
    glyph: summary.glyph
    on: summary.active
    // With a Wi-Fi device the circle switches Wi-Fi; without one there is nothing to switch, so it opens the list.
    onToggled: devices.wirelessDeviceAvailable ? handler.enableWireless(!enabledConnections.wirelessEnabled)
                                              : controlCenter.openPanel("network")
    onOpened: controlCenter.openPanel("network")

    PlasmaNM.NetworkStatus { id: networkStatus }
    PlasmaNM.ConnectionIcon { id: connectionIcon; connectivity: networkStatus.connectivity }
    PlasmaNM.AvailableDevices { id: devices }
    PlasmaNM.EnabledConnections { id: enabledConnections }
    PlasmaNM.Handler { id: handler }
}
