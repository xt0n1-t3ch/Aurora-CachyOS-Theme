// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Quick Look's rules: how a file is shown, and when Space opens it instead of reaching Dolphin's view.

#pragma once

#include <QStringList>
#include <qnamespace.h>

namespace fe {

/// How Quick Look shows a file.
enum class LookKind {
    /// The picture itself, scaled to fit.
    Image,
    /// The start of the text, in a monospaced font.
    Text,
    /// A large preview from KIO's thumbnailers (PDF, video, fonts, ...) or the file's icon, with its details.
    Preview,
};

/// How a file of this type shows; mimeAncestry is its MIME type followed by every type it inherits from.
[[nodiscard]] LookKind lookKindFor(const QStringList &mimeAncestry);

/// Milliseconds after a typed letter during which Dolphin's view adds a Space to the name being typed
/// (KItemListKeyboardSearchManager's timeout).
inline constexpr qint64 TYPE_AHEAD_MS = 1000;

/// Whether Space, pressed with these modifiers msSinceTyped after the last letter typed in the view (negative for
/// never), opens Quick Look. Space with a modifier and Space inside a typed name stay Dolphin's.
[[nodiscard]] bool spaceShowsQuickLook(Qt::KeyboardModifiers modifiers, qint64 msSinceTyped);

} // namespace fe
