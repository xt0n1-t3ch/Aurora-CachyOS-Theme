// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements gitactions.h.

#include "gitactions.h"

#include "core/gitcommands.h"

#include <KMessageBox>

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QTimer>
#include <QUrl>

namespace fe {

namespace {

using git::Command;

QIcon commandIcon(Command command)
{
    const char *name = nullptr;
    switch (command) {
    case Command::Commit:
        name = "vcs-commit";
        break;
    case Command::Pull:
        name = "vcs-pull";
        break;
    case Command::Push:
    case Command::Publish:
        name = "vcs-push";
        break;
    case Command::Fetch:
        name = "view-refresh";
        break;
    case Command::Branches:
        name = "vcs-branch";
        break;
    case Command::Stash:
        name = "vcs-stash";
        break;
    case Command::ApplyStash:
        name = "vcs-stash-pop";
        break;
    case Command::Changes:
    case Command::AllChanges:
        name = "vcs-diff";
        break;
    case Command::History:
    case Command::RepositoryHistory:
        name = "view-history";
        break;
    case Command::OpenRemote:
        name = "internet-services";
        break;
    case Command::AbortOperation:
        name = "dialog-cancel";
        break;
    case Command::ContinueOperation:
        name = "media-playback-start";
        break;
    case Command::Stage:
        name = "list-add";
        break;
    case Command::Unstage:
        name = "list-remove";
        break;
    case Command::Discard:
        name = "edit-undo";
        break;
    case Command::StopTracking:
        name = "edit-clear";
        break;
    case Command::Ignore:
        name = "view-hidden";
        break;
    case Command::Clone:
        name = "cloud-download";
        break;
    case Command::CreateRepository:
        name = "folder-new";
        break;
    }
    return QIcon::fromTheme(QString::fromLatin1(name));
}

void runJob(const GitMenuContext &context, const QString &folder, const QList<QStringList> &steps, const QString &title)
{
    const GitChanged changed = context.changed;
    startGitJob(new GitJob(folder, steps, title), context.window, [changed](bool) {
        if (changed) {
            changed();
        }
    });
}

QString countedItems(const QStringList &paths)
{
    return paths.size() == 1 ? QFileInfo(paths.constFirst()).fileName() : QStringLiteral("%1 items").arg(paths.size());
}

/// The entries of snapshot under the target's paths, each once.
QList<git::StatusEntry> entriesOfTarget(const GitMenuContext &context, const GitSnapshot &snapshot)
{
    QList<git::StatusEntry> entries;
    for (const QString &path : context.target.paths) {
        for (const git::StatusEntry &entry : git::entriesUnder(snapshot.state.status, path)) {
            if (std::none_of(entries.cbegin(), entries.cend(), [&entry](const git::StatusEntry &seen) { return seen.path == entry.path; })) {
                entries.append(entry);
            }
        }
    }
    return entries;
}

/// Adds a line per path to the work tree's .gitignore, starting it on a line of its own.
bool ignorePaths(const QString &workTree, const QStringList &paths)
{
    QFile file(workTree + QStringLiteral("/.gitignore"));
    QByteArray existing;
    if (file.open(QIODevice::ReadOnly)) {
        existing = file.readAll();
        file.close();
    }
    if (!file.open(QIODevice::Append)) {
        return false;
    }
    QByteArray lines = !existing.isEmpty() && !existing.endsWith('\n') ? QByteArrayLiteral("\n") : QByteArray();
    for (const QString &path : paths) {
        lines += git::ignoreLine(path, QFileInfo(workTree + QLatin1Char('/') + path).isDir()).toUtf8() + '\n';
    }
    return file.write(lines) == lines.size();
}

QString defaultRemote(const QList<git::Remote> &remotes)
{
    for (const git::Remote &remote : remotes) {
        if (remote.name == u"origin") {
            return remote.name;
        }
    }
    return remotes.isEmpty() ? QString() : remotes.constFirst().name;
}

} // namespace

QMenu *addGitMenu(QMenu &menu, const GitMenuContext &context)
{
    if (context.target.kind == git::Target::Kind::None) {
        return nullptr;
    }
    menu.addSeparator();
    QMenu *submenu = menu.addMenu(QIcon::fromTheme(QStringLiteral("vcs-branch")), QStringLiteral("Git"));
    submenu->setObjectName(QStringLiteral("gitMenu"));
    if (context.target.kind == git::Target::Kind::Outside) {
        fillGitMenu(submenu, git::outsideMenu(), context, {});
        return submenu;
    }
    submenu->addAction(QStringLiteral("Reading the repository\u2026"))->setEnabled(false);
    QObject::connect(submenu, &QMenu::aboutToShow, submenu, [submenu, context] {
        if (submenu->property("read").toBool()) {
            return;
        }
        submenu->setProperty("read", true);
        const bool items = context.target.kind == git::Target::Kind::Items;
        loadGitSnapshot(context.target.repository, items ? context.target.paths : QStringList(), submenu,
                        [submenu, context, items](const std::optional<GitSnapshot> &snapshot) {
                            submenu->clear();
                            if (!snapshot) {
                                submenu->addAction(QStringLiteral("git could not read this repository"))->setEnabled(false);
                                return;
                            }
                            const git::Menu plan = items ? git::itemsMenu(snapshot->state, snapshot->items) : git::repositoryMenu(snapshot->state);
                            fillGitMenu(submenu, plan, context, *snapshot);
                        });
    });
    return submenu;
}

void fillGitMenu(QMenu *submenu, const git::Menu &plan, const GitMenuContext &context, const GitSnapshot &snapshot)
{
    if (!plan.header.isEmpty()) {
        QAction *header = submenu->addAction(plan.header);
        header->setEnabled(false);
        submenu->addSeparator();
    }
    for (const git::MenuEntry &entry : plan.entries) {
        if (entry.separatorBefore && !submenu->actions().isEmpty() && !submenu->actions().constLast()->isSeparator()) {
            submenu->addSeparator();
        }
        QAction *action = submenu->addAction(commandIcon(entry.command), entry.label);
        action->setEnabled(entry.enabled);
        action->setData(int(entry.command));
        const git::Command command = entry.command;
        QObject::connect(action, &QAction::triggered, submenu, [command, context, snapshot] {
            // After the menu has closed: a sheet or a question must not open inside the menu's own event loop.
            QTimer::singleShot(0, context.window, [command, context, snapshot] { runGitCommand(command, context, snapshot); });
        });
    }
}

void runGitCommand(git::Command command, const GitMenuContext &context, const GitSnapshot &snapshot)
{
    QWidget *window = context.window;
    if (!window) {
        return;
    }
    const git::Repository &repository = context.target.repository;
    const QString workTree = repository.workTree;
    const git::Status &status = snapshot.state.status;
    const QStringList &paths = context.target.paths;
    switch (command) {
    case Command::Commit:
        (new CommitSheet(repository, snapshot, context.changed, window))->open();
        return;
    case Command::Pull:
        runJob(context, workTree, {{QStringLiteral("pull"), QStringLiteral("--progress")}}, QStringLiteral("Pulling from %1").arg(status.upstream));
        return;
    case Command::Push:
        runJob(context, workTree, {{QStringLiteral("push"), QStringLiteral("--progress")}}, QStringLiteral("Pushing to %1").arg(status.upstream));
        return;
    case Command::Publish: {
        const QString remote = defaultRemote(snapshot.state.remotes);
        runJob(context, workTree, {git::publishArguments(remote)}, QStringLiteral("Publishing %1 to %2").arg(status.branch, remote));
        return;
    }
    case Command::Fetch:
        runJob(context, workTree, {{QStringLiteral("fetch"), QStringLiteral("--all"), QStringLiteral("--prune"), QStringLiteral("--progress")}},
               QStringLiteral("Fetching %1").arg(QFileInfo(workTree).fileName()));
        return;
    case Command::Branches:
        (new BranchesSheet(repository, context.changed, window))->open();
        return;
    case Command::Stash:
        runJob(context, workTree, {{QStringLiteral("stash"), QStringLiteral("push"), QStringLiteral("--include-untracked")}},
               QStringLiteral("Stashing the changes of %1").arg(QFileInfo(workTree).fileName()));
        return;
    case Command::ApplyStash:
        runJob(context, workTree, {{QStringLiteral("stash"), QStringLiteral("pop")}}, QStringLiteral("Restoring the latest stash"));
        return;
    case Command::Changes:
        (new ChangesSheet(repository, snapshot, paths, window))->open();
        return;
    case Command::History:
        (new HistorySheet(repository, paths, window))->open();
        return;
    case Command::AllChanges:
        (new ChangesSheet(repository, snapshot, {}, window))->open();
        return;
    case Command::RepositoryHistory:
        (new HistorySheet(repository, {}, window))->open();
        return;
    case Command::OpenRemote:
        for (const git::Remote &remote : snapshot.state.remotes) {
            if (const QString page = git::remoteWebPage(remote.url); !page.isEmpty()) {
                QDesktopServices::openUrl(QUrl(page));
                return;
            }
        }
        return;
    case Command::AbortOperation: {
        if (KMessageBox::warningContinueCancel(window, QStringLiteral("Stop and go back to how things were before it started? What was resolved so far is lost."),
                                               QStringLiteral("Abort"), KGuiItem(QStringLiteral("Abort"), QStringLiteral("dialog-cancel")))
            == KMessageBox::Continue) {
            runJob(context, workTree, {git::abortArguments(snapshot.state.operation)}, QStringLiteral("Aborting"));
        }
        return;
    }
    case Command::ContinueOperation:
        runJob(context, workTree, {git::continueArguments(snapshot.state.operation)}, QStringLiteral("Continuing"));
        return;
    case Command::Stage:
        runJob(context, workTree, {git::stageArguments(paths)}, QStringLiteral("Staging %1").arg(countedItems(paths)));
        return;
    case Command::Unstage:
        runJob(context, workTree, {git::unstageArguments(paths, !status.head.isEmpty())}, QStringLiteral("Unstaging %1").arg(countedItems(paths)));
        return;
    case Command::Discard: {
        const QList<git::StatusEntry> entries = entriesOfTarget(context, snapshot);
        QStringList files;
        for (const git::StatusEntry &entry : entries) {
            if ((entry.kind == git::StatusEntry::Kind::Changed || entry.kind == git::StatusEntry::Kind::Renamed) && (entry.hasStaged() || entry.hasUnstaged())) {
                files.append(entry.path);
            }
        }
        if (KMessageBox::warningContinueCancelList(window,
                                                   QStringLiteral("Put these files back as the last commit has them? Their changes cannot be brought back. "
                                                                  "New files are only unstaged, never deleted."),
                                                   files, QStringLiteral("Discard Changes"), KGuiItem(QStringLiteral("Discard Changes"), QStringLiteral("edit-undo")),
                                                   KStandardGuiItem::cancel(), QString(), KMessageBox::Dangerous)
            == KMessageBox::Continue) {
            runJob(context, workTree, git::discardSteps(entries), QStringLiteral("Discarding the changes of %1").arg(countedItems(paths)));
        }
        return;
    }
    case Command::StopTracking:
        runJob(context, workTree, {git::stopTrackingArguments(paths)}, QStringLiteral("Untracking %1").arg(countedItems(paths)));
        return;
    case Command::Ignore:
        if (!ignorePaths(workTree, paths)) {
            KMessageBox::error(window, QStringLiteral("The .gitignore file of %1 could not be written.").arg(QFileInfo(workTree).fileName()));
        } else if (context.changed) {
            context.changed();
        }
        return;
    case Command::Clone:
        (new CloneSheet(context.target.folder, context.changed, window))->open();
        return;
    case Command::CreateRepository:
        runJob(context, context.target.folder, {{QStringLiteral("init")}}, QStringLiteral("Making %1 a repository").arg(QFileInfo(context.target.folder).fileName()));
        return;
    }
}

} // namespace fe
