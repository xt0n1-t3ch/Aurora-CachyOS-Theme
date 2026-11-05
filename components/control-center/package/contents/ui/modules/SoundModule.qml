// SPDX-License-Identifier: GPL-3.0-only
// Sound: the default output's volume. The speaker mutes; the round button lists the outputs.
import QtQuick
import org.kde.plasma.private.volume
import "../lib"
import "../js/cc.js" as CC

SliderModule {
    id: module

    property Item controlCenter
    readonly property var sink: PreferredDevice.sink
    // PipeWire and PulseAudio name their placeholder sink "auto_null" when there is no real output.
    readonly property bool available: !!sink && sink.name !== "auto_null"
    readonly property int percent: available ? CC.percentFromVolume(sink.volume, PulseAudio.NormalVolume) : 0

    title: "Sound"
    value: available && !sink.muted ? percent / 100 : 0
    startGlyph: CC.speakerGlyph(percent, available && sink.muted)
    endGlyph: "speaker.wave"
    startClickable: available
    outputButton: true
    enabled: available
    onMoved: v => {
        sink.muted = false
        sink.volume = CC.volumeFromPercent(Math.round(v * 100), PulseAudio.NormalVolume)
    }
    onStartClicked: sink.muted = !sink.muted
    onOutputClicked: controlCenter.openPanel("sound")
}
