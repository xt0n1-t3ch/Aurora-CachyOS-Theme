// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Which edges of a frameless window a press resizes.

#pragma once

#include <QPoint>
#include <QSize>
#include <Qt>

namespace fe {

/// The window edges a press at pos resizes, for a frameless window whose resize band is border wide.
[[nodiscard]] Qt::Edges resizeEdgesAt(QPoint pos, QSize size, int border);

} // namespace fe
