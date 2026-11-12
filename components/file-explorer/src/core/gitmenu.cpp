// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements gitmenu.h.

#include "core/gitmenu.h"

#include <algorithm>

namespace fe::git {

namespace {

/// "1 commit", "3 changes": a count with its noun in the right number.
QString counted(int count, const char *one, const char *many)
{
    return QStringLiteral("%1 %2").arg(count).arg(QLatin1String(count == 1 ? one : many));
}

QString operationName(Operation operation)
{
    switch (operation) {
    case Operation::Merge:
        return QStringLiteral("Merge");
    case Operation::Rebase:
        return QStringLiteral("Rebase");
    case Operation::CherryPick:
        return QStringLiteral("Cherry-Pick");
    case Operation::Revert:
        return QStringLiteral("Revert");
    case Operation::None:
        break;
    }
    return {};
}

bool canReachRemote(const RepositoryState &state)
{
    return !state.remotes.isEmpty() && !state.status.branch.isEmpty() && !state.status.head.isEmpty();
}

/// The host part of a web page address, for "Open on github.com".
QString hostOf(const QString &page)
{
    return page.section(QLatin1String("://"), 1).section(QLatin1Char('/'), 0, 0);
}

/// The remote a branch with no upstream would be published to: origin when there is one (git clone's name), else
/// the only remote; empty when there are several and none is origin.
QString publishRemote(const QList<Remote> &remotes)
{
    for (const Remote &remote : remotes) {
        if (remote.name == u"origin") {
            return remote.name;
        }
    }
    return remotes.size() == 1 ? remotes.constFirst().name : QString();
}

void addSyncEntries(QList<MenuEntry> &entries, const RepositoryState &state)
{
    if (!canReachRemote(state)) {
        return;
    }
    const Status &status = state.status;
    if (status.upstream.isEmpty()) {
        const QString remote = publishRemote(state.remotes);
        entries.append({Command::Publish, remote.isEmpty() ? QStringLiteral("Publish Branch") : QStringLiteral("Publish Branch to %1").arg(remote),
                        true, true});
    } else {
        const int behind = status.behind.value_or(0);
        const int ahead = status.ahead.value_or(0);
        // Pull stays offered when git last saw nothing new: the remote may have moved since the last fetch.
        entries.append({Command::Pull, behind > 0 ? QStringLiteral("Pull %1").arg(counted(behind, "Commit", "Commits")) : QStringLiteral("Pull"),
                        status.behind.has_value(), true});
        entries.append({Command::Push, ahead > 0 ? QStringLiteral("Push %1").arg(counted(ahead, "Commit", "Commits")) : QStringLiteral("Nothing to Push"),
                        ahead > 0});
    }
    entries.append({Command::Fetch, QStringLiteral("Fetch"), true});
}

} // namespace

QList<StatusEntry> entriesUnder(const Status &status, const QString &path)
{
    QList<StatusEntry> found;
    const QString folder = path.endsWith(QLatin1Char('/')) ? path : path + QLatin1Char('/');
    for (const StatusEntry &entry : status.entries) {
        // An untracked folder is one entry ending in "/"; a path inside it belongs to it.
        const bool inside = entry.path == path || entry.path.startsWith(folder) || (entry.path.endsWith(QLatin1Char('/')) && path.startsWith(entry.path));
        if (path.isEmpty() || inside) {
            found.append(entry);
        }
    }
    return found;
}

QString stateLine(const RepositoryState &state)
{
    const Status &status = state.status;
    const QString where = status.branch.isEmpty() ? QStringLiteral("Detached at %1").arg(status.head.left(7)) : status.branch;
    if (state.operation != Operation::None) {
        QString line = state.operation == Operation::Merge   ? QStringLiteral("Merging into %1").arg(where)
                       : state.operation == Operation::Rebase ? QStringLiteral("Rebasing %1").arg(where)
                                                              : QStringLiteral("%1 in progress on %2").arg(operationName(state.operation), where);
        if (const int conflicts = status.conflictCount()) {
            line += QStringLiteral(", %1").arg(counted(conflicts, "conflict", "conflicts"));
        }
        return line;
    }
    if (status.head.isEmpty()) {
        return QStringLiteral("%1, no commits yet").arg(where);
    }
    if (status.branch.isEmpty()) {
        return where;
    }
    if (state.remotes.isEmpty()) {
        return QStringLiteral("%1, no remote").arg(where);
    }
    if (status.upstream.isEmpty()) {
        return QStringLiteral("%1, not published").arg(where);
    }
    if (!status.ahead || !status.behind) {
        return QStringLiteral("%1, %2 is gone").arg(where, status.upstream);
    }
    QStringList parts;
    if (*status.ahead > 0) {
        parts.append(QStringLiteral("%1 to push").arg(*status.ahead));
    }
    if (*status.behind > 0) {
        parts.append(QStringLiteral("%1 to pull").arg(*status.behind));
    }
    if (parts.isEmpty()) {
        return QStringLiteral("%1, up to date with %2").arg(where, status.upstream);
    }
    return QStringLiteral("%1, %2").arg(where, parts.join(QStringLiteral(", ")));
}

Menu repositoryMenu(const RepositoryState &state)
{
    const Status &status = state.status;
    Menu menu{stateLine(state), {}};
    QList<MenuEntry> &entries = menu.entries;
    const int changes = status.changeCount();
    const int conflicts = status.conflictCount();
    if (conflicts > 0) {
        entries.append({Command::Commit, QStringLiteral("Resolve %1 First").arg(counted(conflicts, "Conflict", "Conflicts")), false});
    } else if (changes > 0 || state.operation == Operation::Merge) {
        entries.append({Command::Commit, changes > 0 ? QStringLiteral("Commit %1\u2026").arg(counted(changes, "Change", "Changes")) : QStringLiteral("Commit Merge\u2026"), true});
    } else {
        entries.append({Command::Commit, QStringLiteral("Nothing to Commit"), false});
    }
    addSyncEntries(entries, state);

    entries.append({Command::Branches, status.branch.isEmpty() ? QStringLiteral("Switch to a Branch\u2026") : QStringLiteral("Branches\u2026"), !status.head.isEmpty(), true});
    if (state.operation == Operation::None && !status.head.isEmpty()) {
        entries.append({Command::Stash, changes > 0 ? QStringLiteral("Stash %1").arg(counted(changes, "Change", "Changes")) : QStringLiteral("Nothing to Stash"),
                        changes > 0 && conflicts == 0});
        if (status.stashes > 0) {
            entries.append({Command::ApplyStash, QStringLiteral("Restore Latest Stash (%1 saved)").arg(status.stashes), true});
        }
    }

    entries.append({Command::Changes, QStringLiteral("Show Changes\u2026"), changes > 0, true});
    entries.append({Command::History, QStringLiteral("History\u2026"), !status.head.isEmpty()});

    if (state.operation != Operation::None) {
        const QString name = operationName(state.operation);
        entries.append({Command::AbortOperation, QStringLiteral("Abort %1").arg(name), true, true});
        if (state.operation != Operation::Merge) {
            entries.append({Command::ContinueOperation, QStringLiteral("Continue %1").arg(name), conflicts == 0});
        }
    }

    for (const Remote &remote : state.remotes) {
        const QString page = remoteWebPage(remote.url);
        if (!page.isEmpty()) {
            entries.append({Command::OpenRemote, QStringLiteral("Open on %1").arg(hostOf(page)), true, true});
            break;
        }
    }
    return menu;
}

Menu itemsMenu(const RepositoryState &state, const QList<ItemFacts> &items)
{
    bool anyStage = false;
    bool allConflicted = !items.isEmpty();
    bool anyUnstage = false;
    bool anyDiscard = false;
    bool anyChange = false;
    bool anyTracked = false;
    bool anyIgnorable = false;
    for (const ItemFacts &item : items) {
        const QList<StatusEntry> under = entriesUnder(state.status, item.path);
        bool conflicted = false;
        for (const StatusEntry &entry : under) {
            if (entry.kind == StatusEntry::Kind::Ignored) {
                continue;
            }
            anyChange = true;
            anyStage = anyStage || entry.hasUnstaged();
            anyUnstage = anyUnstage || entry.hasStaged();
            conflicted = conflicted || entry.kind == StatusEntry::Kind::Conflicted;
            if ((entry.kind == StatusEntry::Kind::Changed || entry.kind == StatusEntry::Kind::Renamed) && (entry.hasStaged() || entry.hasUnstaged())) {
                anyDiscard = true;
            }
        }
        allConflicted = allConflicted && conflicted;
        anyTracked = anyTracked || item.tracked;
        anyIgnorable = anyIgnorable || (!item.tracked && !item.ignored);
    }

    Menu menu{stateLine(state), {}};
    QList<MenuEntry> &entries = menu.entries;
    entries.append({Command::Stage, allConflicted ? QStringLiteral("Mark as Resolved") : QStringLiteral("Stage"), anyStage});
    entries.append({Command::Unstage, QStringLiteral("Unstage"), anyUnstage});
    entries.append({Command::Discard, QStringLiteral("Discard Changes\u2026"), anyDiscard});
    entries.append({Command::Changes, QStringLiteral("Show Changes\u2026"), anyChange, true});
    entries.append({Command::History, QStringLiteral("History\u2026"), anyTracked && !state.status.head.isEmpty()});
    entries.append({Command::StopTracking, QStringLiteral("Stop Tracking"), anyTracked, true});
    entries.append({Command::Ignore, QStringLiteral("Ignore"), anyIgnorable});

    // The repository's own commands follow, so a commit or a push is never more than one menu away. Their changes and
    // history are the whole repository's, and say so, next to the items' own.
    Menu repository = repositoryMenu(state);
    if (!repository.entries.isEmpty()) {
        repository.entries.first().separatorBefore = true;
    }
    for (MenuEntry &entry : repository.entries) {
        if (entry.command == Command::Changes) {
            entry = {Command::AllChanges, QStringLiteral("Show All Changes\u2026"), entry.enabled, entry.separatorBefore};
        } else if (entry.command == Command::History) {
            entry = {Command::RepositoryHistory, QStringLiteral("Repository History\u2026"), entry.enabled, entry.separatorBefore};
        }
    }
    entries.append(repository.entries);
    return menu;
}

Menu outsideMenu()
{
    return {QString(),
            {{Command::Clone, QStringLiteral("Clone Repository Here\u2026"), true}, {Command::CreateRepository, QStringLiteral("Create Repository Here"), true}}};
}

} // namespace fe::git
