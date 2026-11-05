// SPDX-License-Identifier: GPL-3.0-only
// Frosts every Glass in this window through org.aurora.glass. Without that module the glass is only tinted,
// so the import lives in a file of its own that this Loader may fail to load.
import QtQuick

Loader {
    source: Qt.resolvedUrl("GlassBlurImpl.qml")
}
