// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements gitcommands.h.

#include "core/gitcommands.h"

namespace fe::git {

namespace {

const QString SEPARATOR = QStringLiteral("--");

/// The paths an entry covers in a commit: a rename's new and old paths, so the old one's removal is committed too.
QStringList pathsOf(const StatusEntry &entry)
{
    QStringList paths{entry.path};
    if (!entry.originalPath.isEmpty()) {
        paths.append(entry.originalPath);
    }
    return paths;
}

} // namespace

QStringList stageArguments(const QStringList &paths)
{
    return QStringList{QStringLiteral("add"), QStringLiteral("-A"), SEPARATOR} + paths;
}

QStringList unstageArguments(const QStringList &paths, bool hasHead)
{
    if (!hasHead) {
        return QStringList{QStringLiteral("rm"), QStringLiteral("--cached"), QStringLiteral("-r"), QStringLiteral("-q"), SEPARATOR} + paths;
    }
    return QStringList{QStringLiteral("restore"), QStringLiteral("--staged"), SEPARATOR} + paths;
}

QList<QStringList> discardSteps(const QList<StatusEntry> &entries)
{
    QStringList restore;
    QStringList unstageNew;
    for (const StatusEntry &entry : entries) {
        if (entry.kind != StatusEntry::Kind::Changed && entry.kind != StatusEntry::Kind::Renamed) {
            continue;
        }
        if (entry.staged == QLatin1Char('A')) {
            unstageNew.append(entry.path);
        } else if (entry.hasStaged() || entry.hasUnstaged()) {
            restore.append(pathsOf(entry));
        }
    }
    QList<QStringList> steps;
    if (!restore.isEmpty()) {
        steps.append(QStringList{QStringLiteral("restore"), QStringLiteral("--source=HEAD"), QStringLiteral("--staged"), QStringLiteral("--worktree"), SEPARATOR} +
                     restore);
    }
    if (!unstageNew.isEmpty()) {
        steps.append(QStringList{QStringLiteral("rm"), QStringLiteral("--cached"), QStringLiteral("-q"), SEPARATOR} + unstageNew);
    }
    return steps;
}

QStringList stopTrackingArguments(const QStringList &paths)
{
    return QStringList{QStringLiteral("rm"), QStringLiteral("--cached"), QStringLiteral("-r"), QStringLiteral("-q"), SEPARATOR} + paths;
}

QList<QStringList> commitSteps(const QList<StatusEntry> &chosen, const QString &summary, const QString &description, bool amend, bool merging)
{
    // A staged rename is staged as its new path; the commit also names the old one, which only HEAD still has, so its
    // removal goes in too.
    QStringList staged;
    QStringList paths;
    for (const StatusEntry &entry : chosen) {
        staged.append(entry.path);
        paths.append(pathsOf(entry));
    }
    QStringList commit{QStringLiteral("commit")};
    if (amend && !merging) {
        commit.append(QStringLiteral("--amend"));
    }
    commit.append({QStringLiteral("-m"), summary.trimmed()});
    if (!description.trimmed().isEmpty()) {
        commit.append({QStringLiteral("-m"), description.trimmed()});
    }
    QList<QStringList> steps;
    if (!staged.isEmpty()) {
        steps.append(stageArguments(staged));
    }
    if (!merging && !paths.isEmpty()) {
        // Only the chosen paths, whatever else is staged (git-commit(1), --only, the default with paths).
        commit.append(SEPARATOR);
        commit.append(paths);
    }
    steps.append(commit);
    return steps;
}

QStringList publishArguments(const QString &remote)
{
    return {QStringLiteral("push"), QStringLiteral("--progress"), QStringLiteral("--set-upstream"), remote, QStringLiteral("HEAD")};
}

QStringList abortArguments(Operation operation)
{
    switch (operation) {
    case Operation::Merge:
        return {QStringLiteral("merge"), QStringLiteral("--abort")};
    case Operation::Rebase:
        return {QStringLiteral("rebase"), QStringLiteral("--abort")};
    case Operation::CherryPick:
        return {QStringLiteral("cherry-pick"), QStringLiteral("--abort")};
    case Operation::Revert:
        return {QStringLiteral("revert"), QStringLiteral("--abort")};
    case Operation::None:
        break;
    }
    return {};
}

QStringList continueArguments(Operation operation)
{
    switch (operation) {
    case Operation::Merge:
        return {QStringLiteral("merge"), QStringLiteral("--continue")};
    case Operation::Rebase:
        return {QStringLiteral("rebase"), QStringLiteral("--continue")};
    case Operation::CherryPick:
        return {QStringLiteral("cherry-pick"), QStringLiteral("--continue")};
    case Operation::Revert:
        return {QStringLiteral("revert"), QStringLiteral("--continue")};
    case Operation::None:
        break;
    }
    return {};
}

QStringList switchArguments(const Branch &branch)
{
    if (branch.remote) {
        return {QStringLiteral("switch"), QStringLiteral("--track"), branch.name};
    }
    return {QStringLiteral("switch"), branch.name};
}

QStringList diffArguments(const StatusEntry &entry, bool hasHead)
{
    if (entry.kind == StatusEntry::Kind::Untracked) {
        return {QStringLiteral("diff"), QStringLiteral("--no-index"), SEPARATOR, QStringLiteral("/dev/null"), entry.path};
    }
    QStringList arguments{QStringLiteral("diff")};
    if (entry.kind != StatusEntry::Kind::Conflicted) {
        arguments.append(hasHead ? QStringLiteral("HEAD") : QStringLiteral("--cached"));
    }
    if (entry.kind == StatusEntry::Kind::Renamed) {
        arguments.append(QStringLiteral("-M"));
    }
    arguments.append(SEPARATOR);
    arguments.append(pathsOf(entry));
    return arguments;
}

QStringList logArguments(const QStringList &paths, int limit)
{
    QStringList arguments{QStringLiteral("log"), QStringLiteral("-z"), QStringLiteral("--format=") + logFormat(), QStringLiteral("-n"), QString::number(limit)};
    if (paths.size() == 1) {
        // One file's history goes on through its renames.
        arguments.append(QStringLiteral("--follow"));
    }
    arguments.append(SEPARATOR);
    arguments.append(paths);
    return arguments;
}

QString ignoreLine(const QString &path, bool isFolder)
{
    QString line;
    for (const QChar c : path) {
        if (c == QLatin1Char('*') || c == QLatin1Char('?') || c == QLatin1Char('[') || c == QLatin1Char('\\')) {
            line.append(QLatin1Char('\\'));
        }
        line.append(c);
    }
    // Trailing spaces are dropped unless escaped.
    qsizetype trailing = line.size();
    while (trailing > 0 && line.at(trailing - 1) == QLatin1Char(' ')) {
        --trailing;
    }
    QString escaped = line.left(trailing);
    for (qsizetype i = trailing; i < line.size(); ++i) {
        escaped.append(QStringLiteral("\\ "));
    }
    // A leading slash anchors the pattern at the top, which also keeps "#" and "!" from starting a comment or a
    // negation.
    return QLatin1Char('/') + escaped + (isFolder ? QStringLiteral("/") : QString());
}

QString branchNameProblem(const QString &name)
{
    if (name.trimmed().isEmpty()) {
        return QStringLiteral("A branch needs a name.");
    }
    if (name.startsWith(QLatin1Char('-'))) {
        return QStringLiteral("A branch name cannot start with a dash.");
    }
    if (name == u"HEAD") {
        return QStringLiteral("%1 is a name git keeps for itself.").arg(name);
    }
    for (const QChar c : name) {
        if (c.unicode() < 0x20 || c.unicode() == 0x7f || QStringLiteral(" ~^:?*[\\").contains(c)) {
            return QStringLiteral("A branch name cannot hold spaces or any of ~ ^ : ? * [ \\.");
        }
    }
    if (name.contains(QLatin1String("..")) || name.contains(QLatin1String("@{")) || name.contains(QLatin1String("//"))) {
        return QStringLiteral("A branch name cannot hold \"..\", \"@{\" or \"//\".");
    }
    if (name.endsWith(QLatin1Char('/')) || name.endsWith(QLatin1Char('.')) || name.endsWith(QLatin1String(".lock"))) {
        return QStringLiteral("A branch name cannot end with \"/\", \".\" or \".lock\".");
    }
    for (const QString &part : name.split(QLatin1Char('/'))) {
        if (part.startsWith(QLatin1Char('.'))) {
            return QStringLiteral("No part of a branch name can start with a dot.");
        }
    }
    return {};
}

} // namespace fe::git
