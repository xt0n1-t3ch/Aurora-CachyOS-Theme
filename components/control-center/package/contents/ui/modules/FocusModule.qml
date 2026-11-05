// SPDX-License-Identifier: GPL-3.0-only
// Focus: Plasma's Do Not Disturb, on until turned off.
import QtQuick
import org.kde.notificationmanager as Notifications
import "../lib"

ModulePill {
    id: module

    property Item controlCenter
    // Do Not Disturb is on while a timed setting, an application or screen mirroring inhibits notifications.
    readonly property bool inhibited: settings.notificationsInhibitedUntil.getTime() > Date.now()
        || settings.notificationsInhibitedByApplication
        || (settings.inhibitNotificationsWhenScreensMirrored && settings.screensMirrored)

    // Turning it on inhibits for a year, which stands for until it is turned off. Turning it off clears every source.
    function toggle() {
        if (inhibited) {
            settings.notificationsInhibitedUntil = undefined
            settings.revokeApplicationInhibitions()
            settings.screensMirrored = false
        } else {
            const until = new Date()
            until.setFullYear(until.getFullYear() + 1)
            settings.notificationsInhibitedUntil = until
        }
        settings.save()
    }

    glyph: "moon"
    title: "Focus"
    subtitle: inhibited ? "Do Not Disturb" : ""
    on: inhibited
    onTone: "indigo"
    onToggled: toggle()
    // Focus has no details panel, so the whole pill toggles it.
    onOpened: toggle()

    Notifications.Settings { id: settings }
}
