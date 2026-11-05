// SPDX-License-Identifier: GPL-3.0-only
// Display: one brightness slider for every monitor PowerDevil can dim (external monitors over DDC/CI).
import QtQuick
import org.kde.plasma.private.brightnesscontrolplugin
import "../lib"
import "../js/cc.js" as CC

SliderModule {
    id: module

    property Item controlCenter
    readonly property var displays: {
        const list = []
        for (let i = 0; i < monitors.count; ++i) {
            const monitor = monitors.objectAt(i)
            if (monitor) {
                list.push({name: monitor.name, current: monitor.current, maximum: monitor.maximum})
            }
        }
        return list
    }
    // From 0 to 100, or -1 when no monitor reports a range.
    readonly property int percent: CC.brightnessPercent(displays)
    readonly property bool available: backend.isBrightnessAvailable && percent >= 0
    // While dragging, the slider follows the hand; the monitors catch up over DDC a little later.
    property real pending: -1

    title: "Display"
    startGlyph: "sun.min"
    endGlyph: "sun.max"
    value: pending >= 0 ? pending : Math.max(0, percent) / 100
    onMoved: v => {
        pending = v
        apply.restart()
    }

    ScreenBrightnessControl {
        id: backend
        isSilent: true
    }

    // One object per monitor of the backend's model, with the names cc.js reads.
    Instantiator {
        id: monitors
        model: backend.displays
        delegate: QtObject {
            required property string displayName
            required property int brightness
            required property int maxBrightness
            readonly property string name: displayName
            readonly property int current: brightness
            readonly property int maximum: maxBrightness
        }
    }

    // Debounces the slider: the monitors are written once the hand has paused for 40 ms.
    Timer {
        id: apply
        interval: 40
        onTriggered: {
            const targets = CC.brightnessTargets(Math.round(module.pending * 100), module.displays)
            for (let i = 0; i < targets.length; ++i) {
                backend.setBrightness(targets[i].name, targets[i].value)
            }
            settle.restart()
        }
    }

    // Gives the slider back to the monitors' own values 1.5 s after the last write.
    Timer {
        id: settle
        interval: 1500
        onTriggered: module.pending = -1
    }
}
