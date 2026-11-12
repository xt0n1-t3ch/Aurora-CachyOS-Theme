// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The sheets of File Explorer's Git menu: Commit, Show Changes, Clone, Branches and History, each a window of its
// own over the File Explorer window, in the same design as the Rename and Compare sheets.

#pragma once

#include "core/gitrepo.h"
#include "gitrunner.h"

#include <QDialog>

#include <functional>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QTreeWidget;

namespace fe {

class ChangesList;
class DiffView;

/// Called after a sheet changed the repository, so the window shows its new state.
using GitChanged = std::function<void()>;

/// Choose the changes to commit, read each one's diff, write the message, then commit, or commit and push. The sheet
/// stays open while git works and closes when the commit is made; on an error it stays, with the message kept.
class CommitSheet : public QDialog
{
    Q_OBJECT

public:
    CommitSheet(const git::Repository &repository, const GitSnapshot &snapshot, GitChanged changed, QWidget *parent = nullptr);

    [[nodiscard]] ChangesList *changes() const;
    [[nodiscard]] QLineEdit *summary() const;
    [[nodiscard]] QPlainTextEdit *description() const;
    [[nodiscard]] QCheckBox *amend() const;
    [[nodiscard]] QPushButton *commitButton() const;
    [[nodiscard]] QPushButton *commitAndPushButton() const;
    [[nodiscard]] DiffView *diff() const;

private:
    void showDiffOfCurrent();
    void updateButtons();
    void commit(bool push);

    git::Repository m_repository;
    GitSnapshot m_snapshot;
    GitChanged m_changed;
    ChangesList *m_changes;
    DiffView *m_diff;
    QLineEdit *m_summary;
    QPlainTextEdit *m_description;
    QCheckBox *m_amend;
    QPushButton *m_commit;
    QPushButton *m_commitAndPush;
    bool m_busy = false;
};

/// What changed in a repository, or under the chosen paths of it, file by file with each diff.
class ChangesSheet : public QDialog
{
    Q_OBJECT

public:
    ChangesSheet(const git::Repository &repository, const GitSnapshot &snapshot, const QStringList &paths, QWidget *parent = nullptr);
    [[nodiscard]] ChangesList *changes() const;
    [[nodiscard]] DiffView *diff() const;

private:
    git::Repository m_repository;
    bool m_hasHead;
    ChangesList *m_changes;
    DiffView *m_diff;
};

/// Clones a repository into a folder: its address, the new folder's name (taken from the address until edited), and
/// git's progress in the transfer center.
class CloneSheet : public QDialog
{
    Q_OBJECT

public:
    /// folder is where the clone's own folder is made.
    CloneSheet(QString folder, GitChanged changed, QWidget *parent = nullptr);
    [[nodiscard]] QLineEdit *address() const;
    [[nodiscard]] QLineEdit *name() const;
    [[nodiscard]] QPushButton *cloneButton() const;

protected:
    void accept() override;

private:
    void updateButton();

    QString m_folder;
    GitChanged m_changed;
    QLineEdit *m_address;
    QLineEdit *m_name;
    QLabel *m_problem;
    QPushButton *m_clone;
    bool m_nameEdited = false;
};

/// The branches here and on the remotes: switch to one, make one, merge one into the current branch, delete one.
class BranchesSheet : public QDialog
{
    Q_OBJECT

public:
    BranchesSheet(const git::Repository &repository, GitChanged changed, QWidget *parent = nullptr);
    [[nodiscard]] QTreeWidget *list() const;
    [[nodiscard]] QLineEdit *newName() const;
    [[nodiscard]] QPushButton *createButton() const;
    [[nodiscard]] QPushButton *switchButton() const;
    /// Reads the branches again; finished is emitted when the list shows them.
    void reload();

Q_SIGNALS:
    void finished();

private:
    void updateButtons();
    void run(const QList<QStringList> &steps, const QString &title);
    [[nodiscard]] std::optional<git::Branch> selected() const;

    git::Repository m_repository;
    GitChanged m_changed;
    QList<git::Branch> m_branches;
    QLabel *m_where;
    QLineEdit *m_filter;
    QTreeWidget *m_list;
    QLineEdit *m_newName;
    QLabel *m_problem;
    QPushButton *m_create;
    QPushButton *m_switch;
    QPushButton *m_merge;
    QPushButton *m_delete;
};

/// The commits of a repository, or of the chosen paths, newest first; a chosen commit shows its message and changes.
class HistorySheet : public QDialog
{
    Q_OBJECT

public:
    HistorySheet(const git::Repository &repository, const QStringList &paths, QWidget *parent = nullptr);
    [[nodiscard]] QTreeWidget *list() const;
    [[nodiscard]] DiffView *diff() const;

Q_SIGNALS:
    /// The commits are listed.
    void loaded();

private:
    git::Repository m_repository;
    QStringList m_paths;
    QList<git::Commit> m_commits;
    QTreeWidget *m_list;
    DiffView *m_diff;
};

} // namespace fe
