// SPDX-License-Identifier: GPL-3.0-only
// Night Light in AirDrop's place: KWin's warm screen at night. The circle turns it on or off; the pill opens its
// settings. KWin keeps the switch in kwinrc, so the change is written there and KWin is asked to reload it.
import QtQuick
import org.kde.kcmutils
import org.kde.plasma.workspace.dbus as DBus
import "../lib"

ModulePill {
    id: module

    property Item controlCenter
    readonly property bool active: nightLight.properties.enabled === true

    function refresh() {
        nightLight.update("enabled")
    }

    glyph: "nightlight"
    title: "Night Light"
    subtitle: active ? "On" : "Off"
    on: active
    onTone: "orange"
    toggleEnabled: nightLight.properties.available !== false
    onToggled: {
        controlCenter.run("kwriteconfig6 --file kwinrc --group NightColor --key Active " + (active ? "false" : "true")
                          + " && qdbus6 org.kde.KWin /KWin reconfigure")
        settle.restart()
    }
    onOpened: {
        controlCenter.close()
        KCMLauncher.openSystemSettings("kcm_nightlight")
    }

    DBus.Properties {
        id: nightLight
        busType: DBus.BusType.Session
        service: "org.kde.KWin"
        path: "/org/kde/KWin/NightLight"
        iface: "org.kde.KWin.NightLight"
    }

    // Reads the state back once KWin has had time to reload its configuration.
    Timer {
        id: settle
        interval: 700
        onTriggered: module.refresh()
    }
}
