// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The git command lines behind each of File Explorer's Git commands, in one place, so what a command runs can be read
// and tested without running it.

#pragma once

#include "core/gitstatus.h"

#include <QList>
#include <QStringList>

namespace fe::git {

/// Stages paths as they are now: new, changed and deleted files alike.
[[nodiscard]] QStringList stageArguments(const QStringList &paths);
/// Takes paths out of the next commit and leaves the files as they are; before the first commit nothing is in HEAD
/// to go back to, so the index forgets them instead.
[[nodiscard]] QStringList unstageArguments(const QStringList &paths, bool hasHead);
/// Puts entries back as the last commit has them. A file that is new in the index is only unstaged: it is not in the
/// last commit, and discarding must never delete what the user wrote.
[[nodiscard]] QList<QStringList> discardSteps(const QList<StatusEntry> &entries);
/// Stops tracking paths but keeps the files.
[[nodiscard]] QStringList stopTrackingArguments(const QStringList &paths);

/// What a commit sheet runs: stage the chosen entries, then commit only them with the message. During a merge git
/// commits everything staged, so the steps stage the entries and commit without naming paths. With amend and no
/// entries, only the last commit's message changes.
[[nodiscard]] QList<QStringList> commitSteps(const QList<StatusEntry> &chosen, const QString &summary, const QString &description, bool amend,
                                             bool merging);

[[nodiscard]] QStringList publishArguments(const QString &remote);
/// The arguments that end or go on with an operation under way.
[[nodiscard]] QStringList abortArguments(Operation operation);
[[nodiscard]] QStringList continueArguments(Operation operation);

/// Switches to branch; a remote branch becomes a local one that follows it, as `git switch` does for a name only a
/// remote has.
[[nodiscard]] QStringList switchArguments(const Branch &branch);

/// The arguments that show what changed in entry against the last commit (or the index before the first commit); an
/// untracked file is shown whole, as added.
[[nodiscard]] QStringList diffArguments(const StatusEntry &entry, bool hasHead);

/// The commits that touched paths (the whole repository when empty), newest first, at most limit of them.
[[nodiscard]] QStringList logArguments(const QStringList &paths, int limit);

/// The .gitignore line that matches exactly path (relative to the work tree's top): anchored at the top, with
/// gitignore's special characters escaped (gitignore(5)).
[[nodiscard]] QString ignoreLine(const QString &path, bool isFolder);

/// Why name cannot be a branch name, by git's rules for ref names (git-check-ref-format(1)), or an empty string when
/// it can.
[[nodiscard]] QString branchNameProblem(const QString &name);

} // namespace fe::git
