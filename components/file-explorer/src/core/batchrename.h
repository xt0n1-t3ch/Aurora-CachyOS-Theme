// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The rules of renaming many items at once, as Finder's Rename sheet offers them (replace text, add text, a format with
// numbers or dates) plus a change of case, and the checks that keep a batch from failing halfway.

#pragma once

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

namespace fe {

enum class RenameMode {
    /// Replaces a text (or a regular expression) in each name.
    Replace,
    /// Adds a text before or after each name.
    Add,
    /// Gives every item one name followed or preceded by a number or a date.
    Format,
    /// Changes the letters' case.
    Case,
};

enum class RenamePosition { Before, After };

enum class RenameFormat {
    /// "Photo1", "Photo2": the number as it is.
    Index,
    /// "Photo00001": the number in five digits, so names sort in order anywhere.
    Counter,
    /// "Photo 2026-11-11": each item's own modification day; items of one day in one folder are then numbered.
    Date,
};

enum class RenameCase { Lower, Upper, Title };

/// What the Rename sheet asks for. Only the fields of the chosen mode are read.
struct RenameRule {
    RenameMode mode = RenameMode::Replace;
    // Replace.
    QString find;
    QString replaceWith;
    bool matchCase = false;
    /// find is a regular expression and replaceWith may name its groups as \1, \2.
    bool regularExpression = false;
    // Add.
    QString text;
    RenamePosition position = RenamePosition::After;
    // Format.
    RenameFormat format = RenameFormat::Index;
    QString customName;
    RenamePosition numberPosition = RenamePosition::After;
    int startAt = 1;
    // Case.
    RenameCase letterCase = RenameCase::Lower;
};

/// One item to rename.
struct RenameItem {
    /// The folder it is in, as any string that is equal for equal folders (the folder's URL).
    QString folder;
    QString name;
    QDateTime modified;
};

/// Why an item cannot take its new name.
enum class RenameProblem {
    None,
    Empty,
    /// A name cannot hold "/" and cannot be "." or "..".
    Invalid,
    /// More than 255 bytes, the limit of Linux filesystems.
    TooLong,
    /// Another item of the batch would take the same name in the same folder.
    Duplicate,
    /// The folder already holds an item of that name (another item of the batch counts: the batch renames one item at
    /// a time, so a swap would stop halfway).
    Exists,
};

struct RenameRow {
    QString newName;
    RenameProblem problem = RenameProblem::None;
};

struct RenamePlan {
    /// One row per item, in the items' order.
    QList<RenameRow> rows;
    /// Set when the rule itself is wrong (a bad regular expression); the rows then keep the old names.
    QString ruleError;
    /// How many items get a new name.
    qsizetype changed = 0;
    /// Whether the batch can run: a rule without error, something to change and no row with a problem.
    [[nodiscard]] bool canRun() const;
};

/// The new names of items under rule. namesInFolders holds every name in each item's folder now (by RenameItem::folder);
/// a folder missing from it counts as holding only the items. The extension (the part a file type is known by, such as
/// ".tar.gz") never changes, the way KIO's batch rename keeps it.
[[nodiscard]] RenamePlan planRenames(const QList<RenameItem> &items, const RenameRule &rule,
                                     const QHash<QString, QSet<QString>> &namesInFolders);

/// The name without its extension, and the extension with its dot (empty when it has none). The extension is the
/// file type's suffix that QMimeDatabase knows, so "backup.tar.gz" splits as "backup" and ".tar.gz"; a name that is
/// only a suffix splits as an empty stem and the suffix.
[[nodiscard]] std::pair<QString, QString> splitExtension(const QString &name);

/// What a problem says next to the new name in the sheet.
[[nodiscard]] QString renameProblemText(RenameProblem problem);

} // namespace fe
