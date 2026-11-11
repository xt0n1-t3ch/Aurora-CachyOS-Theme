// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The ranking of the command palette: which commands a few typed letters find, best first.

#pragma once

#include <QList>
#include <QString>
#include <QStringView>

#include <optional>

namespace fe {

/// How well query matches text: its letters in order, case ignored, not necessarily next to each other. Higher is
/// better; a letter that starts a word, follows the last match or starts the text counts more, and a gap counts less.
/// Nothing when the letters are not all there in order. An empty query matches with 0.
[[nodiscard]] std::optional<int> fuzzyScore(QStringView query, QStringView text);

/// A command the palette offers.
struct PaletteEntry {
    /// What it does, as its menu says it ("New Folder").
    QString text;
    /// Where it lives ("File"), matched after the text.
    QString place;
};

/// The positions of the entries query finds, best first; equal scores keep the entries' order.
[[nodiscard]] QList<qsizetype> rankPalette(const QString &query, const QList<PaletteEntry> &entries);

} // namespace fe
