// SPDX-License-Identifier: GPL-3.0-only
// The outputs the Sound module's round button lists; a click makes one the default.
import QtQuick
import org.kde.kcmutils
import org.kde.plasma.private.volume
import "../lib"

DetailPanel {
    id: panel

    property Item controlCenter

    title: "Sound"
    section: "Output"
    emptyText: "No outputs"
    settingsText: "Sound Settings…"
    onSettingsClicked: {
        controlCenter.close()
        KCMLauncher.openSystemSettings("kcm_pulseaudio")
    }

    model: PulseObjectFilterModel {
        sourceModel: SinkModel {}
    }

    delegate: ListRow {
        required property var model
        readonly property var device: model.PulseObject
        readonly property string form: device ? device.formFactor : ""

        glyph: form === "headphone" || form === "headset" ? "headphones" : "speaker.device"
        title: device ? device.description : ""
        checked: !!device && device.default
        onClicked: device.default = true
    }
}
