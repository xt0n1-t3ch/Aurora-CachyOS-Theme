// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// What git says about a repository, read from its machine-readable output: the state of the work tree, the history,
// the branches and the remotes, and which operation (a merge, a rebase) is under way.

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>

#include <optional>

namespace fe::git {

/// One path that `git status` reports.
struct StatusEntry {
    enum class Kind {
        /// Changed in the index, the work tree or both; also added and deleted files.
        Changed,
        /// Renamed or copied; originalPath names where it came from.
        Renamed,
        /// Both sides of a merge changed it and git could not combine them.
        Conflicted,
        Untracked,
        Ignored,
    };
    /// The path relative to the work tree's top, with "/" between folders.
    QString path;
    QString originalPath;
    Kind kind = Kind::Changed;
    /// git's X and Y letters (git-status(1), Short Format): what changed in the index and in the work tree, "." for
    /// nothing.
    QChar staged = QLatin1Char('.');
    QChar unstaged = QLatin1Char('.');
    bool submodule = false;

    [[nodiscard]] bool hasStaged() const;
    [[nodiscard]] bool hasUnstaged() const;
};

/// The work tree's state as `git status --porcelain=v2 --branch --show-stash -z` reports it.
struct Status {
    /// The branch checked out; empty when HEAD is detached.
    QString branch;
    /// The commit HEAD points at; empty before the first commit.
    QString head;
    /// The branch this one follows, such as "origin/main"; empty when it follows none.
    QString upstream;
    /// Commits only here and only on the upstream, when git could compare them (the upstream may be gone).
    std::optional<int> ahead;
    std::optional<int> behind;
    int stashes = 0;
    QList<StatusEntry> entries;

    /// Entries that a commit, a stash or a discard can act on: everything but ignored paths.
    [[nodiscard]] int changeCount() const;
    [[nodiscard]] int conflictCount() const;
    /// Whether anything is staged for the next commit.
    [[nodiscard]] bool hasStaged() const;
};

/// The letter a change is shown with, as code editors show it: U untracked, A added, M modified, D deleted,
/// R renamed, ! conflicted, I ignored.
[[nodiscard]] QChar changeLetter(const StatusEntry &entry);
/// The words for a change, such as "Modified, not staged" or "New, staged".
[[nodiscard]] QString changeName(const StatusEntry &entry);

/// The status in output, or nothing when it is not git's porcelain v2 format.
[[nodiscard]] std::optional<Status> parseStatus(const QByteArray &output);

/// An operation git stopped in the middle of, waiting for the user.
enum class Operation {
    None,
    Merge,
    Rebase,
    CherryPick,
    Revert,
};

/// What the files of gitDir say is under way: MERGE_HEAD, rebase-merge or rebase-apply, CHERRY_PICK_HEAD, REVERT_HEAD
/// (git's wt-status.c, wt_status_get_state).
[[nodiscard]] Operation operationInProgress(const QString &gitDir);

struct Remote {
    QString name;
    QString url;

    friend bool operator==(const Remote &, const Remote &) = default;
};

/// The remotes in `git remote -v` output, each once, with the address it fetches from.
[[nodiscard]] QList<Remote> parseRemotes(const QByteArray &output);

/// The web page of a repository at a remote address (an https, ssh, scp-like or git address), without credentials;
/// empty for a local path or a file address.
[[nodiscard]] QString remoteWebPage(const QString &url);

/// The folder name `git clone` gives a repository from its address: the last part without ".git".
[[nodiscard]] QString cloneFolderName(const QString &url);

struct Commit {
    QString hash;
    QString shortHash;
    QString author;
    QString email;
    QDateTime time;
    QString subject;
};

/// The log format that parseLog reads; pass it to `git log -z`.
[[nodiscard]] QString logFormat();
/// The commits in the output of `git log -z` with logFormat().
[[nodiscard]] QList<Commit> parseLog(const QByteArray &output);

struct Branch {
    /// The short name: "main", or "origin/main" for a remote branch.
    QString name;
    bool remote = false;
    bool current = false;
    QString upstream;
    /// git's comparison with the upstream: "ahead 1, behind 2", "gone", or empty when they match.
    QString track;
    QDateTime time;
};

/// The format that parseBranches reads; pass it to `git for-each-ref` with refs/heads and refs/remotes.
[[nodiscard]] QString branchFormat();
/// The branches in that output, local ones first, each group by most recent commit; a remote's HEAD is left out.
[[nodiscard]] QList<Branch> parseBranches(const QByteArray &output);

} // namespace fe::git
