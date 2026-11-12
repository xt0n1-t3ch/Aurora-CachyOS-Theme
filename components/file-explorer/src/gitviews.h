// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The pieces the Git sheets share: a list of changed files with a colored letter for each kind of change, and a view
// of git's diff output colored line by line.

#pragma once

#include "core/gitstatus.h"

#include <QPlainTextEdit>
#include <QTreeWidget>

namespace fe {

/// git's unified diff output, read-only, in a monospaced font: added lines green, removed lines red, hunk headers in
/// the accent and file headers dimmed.
class DiffView : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit DiffView(QWidget *parent = nullptr);
    /// Shows diff; an empty one shows emptyText instead.
    void setDiff(const QByteArray &diff, const QString &emptyText = QStringLiteral("No changes to show"));
};

/// Changed files, one per row: the letter of the change in its color, then the path (and a rename's old path).
class ChangesList : public QTreeWidget
{
    Q_OBJECT

public:
    /// checkable lists put a box in front of each row, for choosing what a commit takes.
    explicit ChangesList(bool checkable, QWidget *parent = nullptr);

    /// Shows entries; ignored entries are left out. Every box starts ticked.
    void setEntries(const QList<git::StatusEntry> &entries);
    [[nodiscard]] QList<git::StatusEntry> entries() const;
    /// The entries whose boxes are ticked (all of them in a list without boxes).
    [[nodiscard]] QList<git::StatusEntry> checkedEntries() const;
    /// The entry of the current row, if there is one.
    [[nodiscard]] std::optional<git::StatusEntry> currentEntry() const;
    /// Ticks or clears every box; lockChecks keeps them that way, for a merge, which commits everything.
    void setAllChecked(bool checked, bool lockChecks = false);

Q_SIGNALS:
    void currentEntryChanged();
    void checksChanged();

private:
    QList<git::StatusEntry> m_entries;
    bool m_checkable;
};

} // namespace fe
