// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The columns view's columns: one list per folder on the way to the folder on screen, Finder's columns, left of
// Dolphin's view, which is the last column. Which folders they are comes from core/paths (columnFolders).

#pragma once

#include <QUrl>
#include <QWidget>

class QHBoxLayout;
class QListView;
class QScrollArea;

namespace fe {

class FolderColumn;

/// A row of folder columns that follows the tab's folder. Each column lists a folder, KIO keeping it current, with the
/// next folder on the way marked; a click on a folder opens it in the tab, a click on a file opens its folder with the
/// file selected. Files dropped on a folder go into it as KIO's drop menu says; items drag out as from Dolphin's view.
class ColumnsStrip : public QWidget
{
    Q_OBJECT

public:
    /// homePath is where the columns start for folders inside it; elsewhere they start at the root of the disk.
    explicit ColumnsStrip(const QString &homePath, QWidget *parent = nullptr);

    /// Shows the columns that lead to folder. Columns already there for the same folders stay, with their scroll.
    void setFolder(const QUrl &folder);
    /// Lists hidden files too, as the view does when it shows them.
    void setShowHiddenFiles(bool show);
    /// The folders shown as columns, outermost first.
    [[nodiscard]] QList<QUrl> folders() const;
    /// The list of the column at index, for tests.
    [[nodiscard]] QListView *column(qsizetype index) const;

Q_SIGNALS:
    /// A folder of a column was clicked.
    void folderChosen(const QUrl &folder);
    /// A file of a column was clicked: its folder opens with it selected.
    void fileChosen(const QUrl &file);

private:
    QString m_homePath;
    QUrl m_folder;
    bool m_showHidden = false;
    QScrollArea *m_scroll;
    QWidget *m_row;
    QHBoxLayout *m_layout;
    QList<FolderColumn *> m_columns;
};

} // namespace fe
