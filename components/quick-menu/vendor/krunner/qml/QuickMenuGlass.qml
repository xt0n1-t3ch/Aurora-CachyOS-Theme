/*
    SPDX-FileCopyrightText: 2026 xt0n1-t3ch
    SPDX-License-Identifier: GPL-2.0-or-later
*/
// Quick Menu's Liquid Glass, the material of the Control Center and the notification banners (glass.frag): frosted by
// KWin behind its rounded shape (the BlurBehind in RunCommand.qml finds it through glassBlur), tinted, with a rim
// lit from above and a soft shadow drawn into the window's transparent margin.
import QtQuick

Item {
    id: glass

    property real radius: 26
    property real shadowSize: 18
    readonly property bool glassBlur: true

    ShaderEffect {
        readonly property real pad: Math.ceil(glass.shadowSize * 1.6)
        z: -1
        x: -pad
        y: -pad
        width: glass.width + 2 * pad
        height: glass.height + 2 * pad
        property size boxSize: Qt.size(glass.width, glass.height)
        property real radius: glass.radius
        // Between the Control Center's clear modules and the banners' dense glass (0.78): Quick Menu floats over
        // whatever is open, white pages included, and its white text has to stay readable there.
        property vector4d tint: Qt.vector4d(0.12, 0.12, 0.14, 0.74)
        property vector4d rimLight: Qt.vector4d(1, 1, 1, 0.5)
        property vector4d rimShade: Qt.vector4d(1, 1, 1, 0.08)
        property real rimWidth: 1.0
        property real sheen: 0.05
        property real shadowAlpha: 0.32
        property real shadowSize: glass.shadowSize
        fragmentShader: Qt.resolvedUrl("shaders/glass.frag.qsb")
    }
}
