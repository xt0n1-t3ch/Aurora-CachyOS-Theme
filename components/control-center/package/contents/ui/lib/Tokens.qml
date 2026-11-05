// SPDX-License-Identifier: GPL-3.0-only
// The Control Center's look in the dark appearance: Liquid Glass tints, text and motion, matched by eye to
// captures of macOS Tahoe's Control Center over dark wallpapers.
pragma Singleton
import QtQuick
import org.kde.kirigami as Kirigami

QtObject {
    // Glass, as straight-alpha rgba for the shader. The blur behind it does most of the work.
    readonly property vector4d glassTint: Qt.vector4d(0.22, 0.22, 0.25, 0.30)
    readonly property vector4d onTint: Qt.vector4d(1, 1, 1, 0.94)
    readonly property vector4d softTint: Qt.vector4d(1, 1, 1, 0.16)
    // The rim is rimLight where the edge faces up and rimShade where it faces down. rimWidth and shadowSize are in px,
    // sheen is the light added along the top edge of the body, and shadowAlpha is the opacity of the shadow.
    readonly property vector4d rimLight: Qt.vector4d(1, 1, 1, 0.55)
    readonly property vector4d rimShade: Qt.vector4d(1, 1, 1, 0.09)
    readonly property real rimWidth: 1.0
    readonly property real sheen: 0.06
    readonly property real shadowAlpha: 0.20
    readonly property real shadowSize: 14

    readonly property color text: "#ffffff"
    readonly property color secondaryText: Qt.rgba(1, 1, 1, 0.62)
    readonly property color sectionText: Qt.rgba(1, 1, 1, 0.46)
    readonly property color separator: Qt.rgba(1, 1, 1, 0.12)
    readonly property color rowHover: Qt.rgba(1, 1, 1, 0.08)
    readonly property color track: Qt.rgba(1, 1, 1, 0.22)
    readonly property color trackFill: "#ffffff"
    readonly property color accent: "#0a84ff"

    readonly property string font: Kirigami.Theme.defaultFont.family
    // Font sizes, in px.
    readonly property int titleSize: 13
    readonly property int subtitleSize: 12
    readonly property int sectionSize: 11

    // Animation durations, in ms.
    readonly property int fast: 120
    readonly property int medium: 200

    // A tint made lighter by hover (lift 0.5) and press (lift 1).
    function lifted(tint, lift) {
        return Qt.vector4d(tint.x + 0.10 * lift, tint.y + 0.10 * lift, tint.z + 0.10 * lift, tint.w + 0.08 * lift)
    }
}
