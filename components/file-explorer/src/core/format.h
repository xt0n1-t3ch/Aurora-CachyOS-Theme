// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Text for the status row and for keyboard shortcuts, written the way Windows users read them.

#pragma once

#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace fe {

/// What the status row knows about the folder on screen.
struct FolderCounts {
    qsizetype items = 0;
    qsizetype selected = 0;
    /// The total size of the selected files; selected folders add nothing.
    qint64 selectedFileBytes = 0;
    /// Free space for the folder, when it is known.
    std::optional<qint64> availableBytes;
};

/// The status row text: "5 items, 120 GB available" or "1 of 32 selected, 41 kB". Sizes are written
/// by formatSize; the app passes the formatter of Dolphin's Size column, so the numbers always match.
[[nodiscard]] QString statusLine(const FolderCounts &counts, const std::function<QString(qint64)> &formatSize);

/// The count a filter row shows: "No matches", "1 match", "12 matches".
[[nodiscard]] QString filterMatchText(qsizetype matches);

/// The title of the context menu's version control submenu: the word every action starts with ("Git" for "Git
/// Commit…", "Git Push…"), or "Version Control" when they do not share one.
[[nodiscard]] QString versionControlMenuTitle(const QStringList &actionTexts);

/// A shortcut as Qt writes it ("Shift+Ctrl+N", "Alt+Return", "Meta+E") written the way Windows shows it
/// ("Ctrl+Shift+N", "Alt+Enter", "Win+E"): modifiers in Win, Ctrl, Alt, Shift order, then the key's Windows name.
[[nodiscard]] QString shortcutLabel(const QString &portableText);

} // namespace fe
