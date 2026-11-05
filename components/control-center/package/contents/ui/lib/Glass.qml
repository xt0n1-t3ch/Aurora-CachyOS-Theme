// SPDX-License-Identifier: GPL-3.0-only
// One piece of Liquid Glass: frosted by KWin behind its rounded shape (BlurBehind finds it through glassBlur),
// tinted, with a rim lit from above and a soft shadow around it. Children sit on the glass.
import QtQuick

Item {
    id: glass

    // Corner radius in px; the default makes a circle or a capsule.
    property real radius: Math.min(width, height) / 2
    // Straight-alpha rgba of the glass body.
    property vector4d tint: Tokens.glassTint
    // From 0 to 1: how much hover (0.5) and press (1) lighten the tint.
    property real lift: 0
    // Whether KWin blurs behind this piece. A piece drawn on top of another one turns it off.
    property bool frosted: true
    // Whether the piece casts a shadow.
    property bool shadowed: true
    // The marker BlurBehind looks for, so that only frosted pieces are blurred.
    readonly property bool glassBlur: frosted

    Behavior on lift {
        NumberAnimation { duration: Tokens.fast; easing.type: Easing.OutCubic }
    }

    ShaderEffect {
        readonly property real pad: glass.shadowed ? Math.ceil(Tokens.shadowSize * 1.6) : 1
        z: -1
        x: -pad
        y: -pad
        width: glass.width + 2 * pad
        height: glass.height + 2 * pad
        property size boxSize: Qt.size(glass.width, glass.height)
        property real radius: glass.radius
        property vector4d tint: glass.lift > 0 ? Tokens.lifted(glass.tint, glass.lift) : glass.tint
        property vector4d rimLight: Tokens.rimLight
        property vector4d rimShade: Tokens.rimShade
        property real rimWidth: Tokens.rimWidth
        property real sheen: Tokens.sheen
        property real shadowAlpha: glass.shadowed ? Tokens.shadowAlpha : 0
        property real shadowSize: Tokens.shadowSize
        fragmentShader: Qt.resolvedUrl("../shaders/glass.frag.qsb")
    }
}
