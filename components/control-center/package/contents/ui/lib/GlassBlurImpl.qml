// SPDX-License-Identifier: GPL-3.0-only
// Blurs behind every Glass of the window (org.aurora.glass) and saturates the blurred backdrop by 1.6, where 1 would
// leave it alone. GlassBlur loads this file, so the Control Center still opens when the module is missing.
import QtQuick
import org.aurora.glass

BlurBehind {
    saturation: 1.6
}
