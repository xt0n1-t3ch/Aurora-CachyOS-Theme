// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// What File Explorer's Git menu offers, decided from the repository's state alone: a line that says where the
// repository stands, and each command with words that say what it will do and whether it can.

#pragma once

#include "core/gitstatus.h"

#include <QList>
#include <QString>

namespace fe::git {

enum class Command {
    Commit,
    Pull,
    Push,
    /// Pushes a branch that follows no remote branch yet, and makes it follow the one it creates.
    Publish,
    Fetch,
    Branches,
    Stash,
    ApplyStash,
    /// What changed in the selected items, or in the repository when nothing is selected.
    Changes,
    /// The commits of the selected items, or of the repository when nothing is selected.
    History,
    /// What changed in the whole repository, from an item's menu.
    AllChanges,
    /// The commits of the whole repository, from an item's menu.
    RepositoryHistory,
    OpenRemote,
    AbortOperation,
    ContinueOperation,
    Stage,
    Unstage,
    Discard,
    StopTracking,
    Ignore,
    Clone,
    CreateRepository,
};

struct MenuEntry {
    Command command;
    QString label;
    bool enabled = true;
    /// A line before this entry, between groups.
    bool separatorBefore = false;
};

struct Menu {
    /// Where the repository stands, shown greyed at the top; empty outside a repository.
    QString header;
    QList<MenuEntry> entries;
};

/// Everything the menu is decided from.
struct RepositoryState {
    Status status;
    QList<Remote> remotes;
    Operation operation = Operation::None;
};

/// A selected file or folder, by its path relative to the work tree's top.
struct ItemFacts {
    QString path;
    /// git knows at least one file at or below the path (`git ls-files`).
    bool tracked = false;
    /// A .gitignore rule matches the path (`git check-ignore`).
    bool ignored = false;
};

/// The line at the top of the menu: the branch, how it stands against its upstream, and an operation under way, such
/// as "main, 2 to push, 1 to pull" or "Merging into main, 2 conflicts".
[[nodiscard]] QString stateLine(const RepositoryState &state);

/// The menu of the repository itself: for the folder on screen inside a work tree, or a folder that is a work tree's
/// top.
[[nodiscard]] Menu repositoryMenu(const RepositoryState &state);

/// The menu of files and folders inside a work tree: what can be done to them, then the repository's own commands.
[[nodiscard]] Menu itemsMenu(const RepositoryState &state, const QList<ItemFacts> &items);

/// The menu of a folder outside every repository, on its background: clone one into it, or make it one.
[[nodiscard]] Menu outsideMenu();

/// The entries of state that concern path: the path itself or anything below it.
[[nodiscard]] QList<StatusEntry> entriesUnder(const Status &status, const QString &path);

} // namespace fe::git
