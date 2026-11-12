// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// File Explorer's Git menu in the context menu: built from the repository's state when it opens, and what each of
// its commands does.

#pragma once

#include "core/gitmenu.h"
#include "core/gitrepo.h"
#include "gitrunner.h"
#include "gitsheets.h"

#include <QPointer>

class QMenu;
class QWidget;

namespace fe {

struct GitMenuContext {
    git::Target target;
    /// The window the sheets and messages open over.
    QPointer<QWidget> window;
    /// Shows the repository's new state after a command changed it.
    GitChanged changed;
};

/// Adds a "Git" submenu to menu for context's target and returns it; it reads the repository when it is first opened
/// and fills itself then. A target of kind None adds nothing and returns null.
QMenu *addGitMenu(QMenu &menu, const GitMenuContext &context);

/// Fills submenu with plan's header and entries, each running its command for context with snapshot.
void fillGitMenu(QMenu *submenu, const git::Menu &plan, const GitMenuContext &context, const GitSnapshot &snapshot);

/// Runs command for context, with what snapshot says of the repository: a job, a sheet or a question first.
void runGitCommand(git::Command command, const GitMenuContext &context, const GitSnapshot &snapshot);

} // namespace fe
