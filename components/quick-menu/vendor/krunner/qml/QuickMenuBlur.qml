/*
    SPDX-FileCopyrightText: 2026 xt0n1-t3ch
    SPDX-License-Identifier: GPL-2.0-or-later
*/
// KWin's blur behind Quick Menu's glass (org.aurora.glass, the Control Center's module). RunCommand.qml loads it
// through a Loader, so Quick Menu still opens, unblurred, if the module is missing.
import QtQuick
import org.aurora.glass

BlurBehind {
    saturation: 1.6
}
