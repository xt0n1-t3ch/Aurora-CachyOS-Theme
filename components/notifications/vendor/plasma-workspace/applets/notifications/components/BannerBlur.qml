/*
    SPDX-FileCopyrightText: 2026 xt0n1-t3ch
    SPDX-License-Identifier: GPL-2.0-only OR GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
// Frosts every BannerGlass in this window through org.aurora.glass. Without that module the banner is only
// tinted, so the import lives in a file of its own that this Loader may fail to load.
import QtQuick

Loader {
    source: Qt.resolvedUrl("BannerBlurImpl.qml")
}
