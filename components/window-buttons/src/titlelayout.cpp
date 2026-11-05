// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Computes the capsule, button and caption rectangles of the title bar from plain numbers.

#include "titlelayout.h"

#include <algorithm>

namespace aurora {

TitleLayout layoutTitleBar(qreal windowWidth, qreal titleBarHeight, int buttonCount, bool edgeToEdge)
{
    TitleLayout layout;
    if (buttonCount <= 0) {
        return layout;
    }
    const qreal width = 2 * CAPSULE_PADDING + buttonCount * SEGMENT_WIDTH;
    const qreal top = (titleBarHeight - CAPSULE_HEIGHT) / 2;
    layout.capsule = QRectF(windowWidth - SIDE_MARGIN - width, top, width, CAPSULE_HEIGHT);

    for (int i = 0; i < buttonCount; ++i) {
        const QRectF segment(layout.capsule.left() + CAPSULE_PADDING + i * SEGMENT_WIDTH, top, SEGMENT_WIDTH, CAPSULE_HEIGHT);
        layout.buttons.append(segment);
        QRectF hit = segment;
        if (edgeToEdge) {
            hit.setTop(0);
            if (i == buttonCount - 1) {
                hit.setRight(windowWidth);
            }
        }
        layout.hitAreas.append(hit);
    }
    return layout;
}

QRectF captionRect(qreal windowWidth, qreal titleBarHeight, qreal textWidth, const QRectF &capsule)
{
    const qreal right = capsule.isEmpty() ? windowWidth - CAPTION_GAP : capsule.left() - CAPTION_GAP;
    const qreal left = CAPTION_GAP;
    const qreal available = std::max<qreal>(0, right - left);
    const qreal width = std::min(textWidth, available);
    // Centred on the whole window, as macOS centres titles, then pushed left of the buttons. The upper
    // bound never drops below the lower one, which std::clamp requires (a window narrower than its buttons).
    const qreal x = std::clamp((windowWidth - width) / 2, left, std::max(left, right - width));
    return {x, 0, width, titleBarHeight};
}

} // namespace aurora
