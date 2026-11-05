// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements edges.h.

#include "core/edges.h"

namespace fe {

Qt::Edges resizeEdgesAt(QPoint pos, QSize size, int border)
{
    Qt::Edges edges;
    if (pos.x() < 0 || pos.y() < 0 || pos.x() >= size.width() || pos.y() >= size.height()) {
        return edges;
    }
    if (pos.x() < border) {
        edges |= Qt::LeftEdge;
    } else if (pos.x() >= size.width() - border) {
        edges |= Qt::RightEdge;
    }
    if (pos.y() < border) {
        edges |= Qt::TopEdge;
    } else if (pos.y() >= size.height() - border) {
        edges |= Qt::BottomEdge;
    }
    return edges;
}

} // namespace fe
