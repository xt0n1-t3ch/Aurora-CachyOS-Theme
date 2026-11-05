// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements quicklook.h.

#include "core/quicklook.h"

namespace fe {

LookKind lookKindFor(const QStringList &mimeAncestry)
{
    for (const QString &type : mimeAncestry) {
        if (type.startsWith(QStringLiteral("image/"))) {
            return LookKind::Image;
        }
        // JSON does not inherit text/plain in shared-mime-info, yet it reads as text.
        if (type == u"text/plain" || type == u"application/json") {
            return LookKind::Text;
        }
    }
    return LookKind::Preview;
}

bool spaceShowsQuickLook(Qt::KeyboardModifiers modifiers, qint64 msSinceTyped)
{
    const bool typing = msSinceTyped >= 0 && msSinceTyped <= TYPE_AHEAD_MS;
    return (modifiers & ~Qt::KeypadModifier) == Qt::NoModifier && !typing;
}

} // namespace fe
