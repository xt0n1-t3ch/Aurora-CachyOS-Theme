// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Draws the soft shadow KWin puts around frameless translucent windows.

#pragma once

class QWidget;

namespace fe {

/// The shape of a soft drop shadow that KWin draws around a frameless, translucent window.
struct ShadowSpec {
    int radius;  ///< The corner radius of the window body the shadow surrounds.
    int extent;  ///< How far the shadow reaches past the window edge.
    int offsetY; ///< How far the light source pushes the shadow down.
    int blur;    ///< The box blur radius; three passes of it approximate a Gaussian.
    int alpha;   ///< The darkness of the shadow right under the body, 0 to 255.
};

/// Asks KWin to draw a shadow around window, replacing the one attached before. The window must
/// have its native window (call it from showEvent or later).
void attachShadow(QWidget *window, const ShadowSpec &spec);

/// Removes the shadow attachShadow added, if any.
void detachShadow(QWidget *window);

} // namespace fe
