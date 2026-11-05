/*
    SPDX-FileCopyrightText: 2026 xt0n1-t3ch
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
// The notification banner's Liquid Glass, the same material as the Control Center's modules (its Glass.qml and
// glass.frag): frosted by KWin behind its rounded shape (BannerBlur finds it through glassBlur), tinted, with a rim
// lit from above and a soft shadow drawn into the window's transparent margin.
import QtQuick

Item {
    id: glass

    property real radius: 22
    readonly property bool glassBlur: true

    ShaderEffect {
        readonly property real pad: Math.ceil(shadowSize * 1.6)
        z: -1
        x: -pad
        y: -pad
        width: glass.width + 2 * pad
        height: glass.height + 2 * pad
        property size boxSize: Qt.size(glass.width, glass.height)
        property real radius: glass.radius
        // Darker and denser than the Control Center's modules: a banner lands on any window, white pages included,
        // and its white text has to stay readable there, as macOS's notification material does.
        property vector4d tint: Qt.vector4d(0.13, 0.13, 0.15, 0.78)
        property vector4d rimLight: Qt.vector4d(1, 1, 1, 0.55)
        property vector4d rimShade: Qt.vector4d(1, 1, 1, 0.09)
        property real rimWidth: 1.0
        property real sheen: 0.06
        property real shadowAlpha: 0.22
        property real shadowSize: 12
        fragmentShader: Qt.resolvedUrl("../shaders/glass.frag.qsb")
    }
}
