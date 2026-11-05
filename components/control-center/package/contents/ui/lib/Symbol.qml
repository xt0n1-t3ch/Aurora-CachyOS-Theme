// SPDX-License-Identifier: GPL-3.0-only
// A Control Center glyph (tools/cc_glyphs.py) in one of its colors: light, dark, blue, indigo or orange.
import QtQuick
import QtQuick.Window

Image {
    // The glyph's file name in icons/<tone>/, without the extension.
    property string name
    property string tone: "light"

    width: 20
    height: 20
    source: name.length ? Qt.resolvedUrl("../icons/" + tone + "/" + name + ".svg") : ""
    // Rasterized at the screen's pixel ratio so the glyph stays sharp.
    sourceSize: Qt.size(Math.ceil(width * Screen.devicePixelRatio), Math.ceil(height * Screen.devicePixelRatio))
    fillMode: Image.PreserveAspectFit
    smooth: true
}
