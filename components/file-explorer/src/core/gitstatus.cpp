// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements gitstatus.h.

#include "core/gitstatus.h"

#include <QFileInfo>
#include <QRegularExpression>
#include <QUrl>

#include <algorithm>

namespace fe::git {

namespace {

/// The separators of logFormat() and branchFormat(): ASCII's unit separator, which no name or subject holds.
constexpr char FIELD = '\x1f';

/// The path after `count` space-separated fields of a porcelain v2 line; paths keep their own spaces.
QString pathAfterFields(const QByteArray &line, int count)
{
    qsizetype at = 0;
    for (int field = 0; field < count; ++field) {
        at = line.indexOf(' ', at);
        if (at < 0) {
            return {};
        }
        ++at;
    }
    return QString::fromUtf8(line.mid(at));
}

bool isChange(QChar letter)
{
    return letter != QLatin1Char('.') && letter != QLatin1Char(' ');
}

} // namespace

bool StatusEntry::hasStaged() const
{
    return (kind == Kind::Changed || kind == Kind::Renamed) && isChange(staged);
}

bool StatusEntry::hasUnstaged() const
{
    return kind == Kind::Untracked || kind == Kind::Conflicted || ((kind == Kind::Changed || kind == Kind::Renamed) && isChange(unstaged));
}

int Status::changeCount() const
{
    return int(std::count_if(entries.cbegin(), entries.cend(), [](const StatusEntry &entry) { return entry.kind != StatusEntry::Kind::Ignored; }));
}

int Status::conflictCount() const
{
    return int(std::count_if(entries.cbegin(), entries.cend(), [](const StatusEntry &entry) { return entry.kind == StatusEntry::Kind::Conflicted; }));
}

bool Status::hasStaged() const
{
    return std::any_of(entries.cbegin(), entries.cend(), [](const StatusEntry &entry) { return entry.hasStaged(); });
}

QChar changeLetter(const StatusEntry &entry)
{
    switch (entry.kind) {
    case StatusEntry::Kind::Untracked:
        return QLatin1Char('U');
    case StatusEntry::Kind::Ignored:
        return QLatin1Char('I');
    case StatusEntry::Kind::Conflicted:
        return QLatin1Char('!');
    case StatusEntry::Kind::Renamed:
        return QLatin1Char('R');
    case StatusEntry::Kind::Changed:
        break;
    }
    if (entry.staged == QLatin1Char('A') || entry.unstaged == QLatin1Char('A')) {
        return QLatin1Char('A');
    }
    if (entry.staged == QLatin1Char('D') || entry.unstaged == QLatin1Char('D')) {
        return QLatin1Char('D');
    }
    return QLatin1Char('M');
}

QString changeName(const StatusEntry &entry)
{
    QString kind;
    switch (changeLetter(entry).unicode()) {
    case 'U':
        return QStringLiteral("New, not tracked yet");
    case 'I':
        return QStringLiteral("Ignored");
    case '!':
        return QStringLiteral("In conflict");
    case 'R':
        kind = QStringLiteral("Renamed");
        break;
    case 'A':
        kind = QStringLiteral("New");
        break;
    case 'D':
        kind = QStringLiteral("Deleted");
        break;
    default:
        kind = QStringLiteral("Modified");
        break;
    }
    if (entry.hasStaged() && entry.hasUnstaged()) {
        return kind + QStringLiteral(", partly staged");
    }
    return kind + (entry.hasStaged() ? QStringLiteral(", staged") : QStringLiteral(", not staged"));
}

std::optional<Status> parseStatus(const QByteArray &output)
{
    Status status;
    bool sawBranch = false;
    const QList<QByteArray> records = output.split('\0');
    for (qsizetype i = 0; i < records.size(); ++i) {
        const QByteArray &record = records.at(i);
        if (record.isEmpty()) {
            continue;
        }
        if (record.startsWith("# ")) {
            const QList<QByteArray> words = record.mid(2).split(' ');
            const QByteArray key = words.value(0);
            const QString value = QString::fromUtf8(record.mid(2 + key.size() + 1));
            if (key == "branch.oid") {
                sawBranch = true;
                status.head = value == u"(initial)" ? QString() : value;
            } else if (key == "branch.head") {
                status.branch = value == u"(detached)" ? QString() : value;
            } else if (key == "branch.upstream") {
                status.upstream = value;
            } else if (key == "branch.ab" && words.size() == 3) {
                status.ahead = words.at(1).mid(1).toInt();
                status.behind = words.at(2).mid(1).toInt();
            } else if (key == "stash") {
                status.stashes = value.toInt();
            }
            continue;
        }
        StatusEntry entry;
        const char type = record.at(0);
        if (type == '?' || type == '!') {
            entry.kind = type == '?' ? StatusEntry::Kind::Untracked : StatusEntry::Kind::Ignored;
            entry.path = QString::fromUtf8(record.mid(2));
        } else if ((type == '1' || type == '2' || type == 'u') && record.size() > 4) {
            entry.staged = QLatin1Char(record.at(2));
            entry.unstaged = QLatin1Char(record.at(3));
            entry.submodule = record.size() > 5 && record.at(5) == 'S';
            if (type == '1') {
                entry.path = pathAfterFields(record, 8);
            } else if (type == '2') {
                entry.kind = StatusEntry::Kind::Renamed;
                entry.path = pathAfterFields(record, 9);
                // With -z the path a rename came from is the next record.
                entry.originalPath = QString::fromUtf8(records.value(++i));
            } else {
                entry.kind = StatusEntry::Kind::Conflicted;
                entry.path = pathAfterFields(record, 10);
            }
        } else {
            return std::nullopt;
        }
        if (entry.path.isEmpty()) {
            return std::nullopt;
        }
        status.entries.append(entry);
    }
    if (!sawBranch) {
        return std::nullopt;
    }
    return status;
}

Operation operationInProgress(const QString &gitDir)
{
    const auto exists = [&gitDir](const char *name) { return QFileInfo::exists(gitDir + QLatin1Char('/') + QLatin1String(name)); };
    if (exists("rebase-merge") || exists("rebase-apply")) {
        return Operation::Rebase;
    }
    if (exists("MERGE_HEAD")) {
        return Operation::Merge;
    }
    if (exists("CHERRY_PICK_HEAD")) {
        return Operation::CherryPick;
    }
    if (exists("REVERT_HEAD")) {
        return Operation::Revert;
    }
    return Operation::None;
}

QList<Remote> parseRemotes(const QByteArray &output)
{
    QList<Remote> remotes;
    for (const QByteArray &line : output.split('\n')) {
        const qsizetype tab = line.indexOf('\t');
        if (tab <= 0 || !line.endsWith(" (fetch)")) {
            continue;
        }
        const Remote remote{QString::fromUtf8(line.left(tab)), QString::fromUtf8(line.mid(tab + 1).chopped(8))};
        if (!remotes.contains(remote)) {
            remotes.append(remote);
        }
    }
    return remotes;
}

QString remoteWebPage(const QString &url)
{
    QString host;
    QString path;
    // scp-like: [user@]host:path, which has no scheme and no slash before the colon.
    static const QRegularExpression scpLike(QStringLiteral("^(?:[^@/]+@)?([^:/]+):(?!//)(.+)$"));
    if (const QRegularExpressionMatch match = scpLike.match(url); match.hasMatch() && !url.contains(QLatin1String("://"))) {
        host = match.captured(1);
        path = match.captured(2);
    } else {
        const QUrl parsed(url);
        const QString scheme = parsed.scheme();
        if (scheme != u"https" && scheme != u"http" && scheme != u"ssh" && scheme != u"git" && scheme != u"git+ssh") {
            return {};
        }
        host = parsed.host();
        path = parsed.path();
        if (scheme == u"http") {
            host = QStringLiteral("http://") + host + (parsed.port() > 0 ? QStringLiteral(":%1").arg(parsed.port()) : QString());
        } else if (scheme == u"https" && parsed.port() > 0 && parsed.port() != 443) {
            host += QStringLiteral(":%1").arg(parsed.port());
        }
    }
    while (path.startsWith(QLatin1Char('/'))) {
        path.remove(0, 1);
    }
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    if (path.endsWith(QLatin1String(".git"))) {
        path.chop(4);
    }
    if (host.isEmpty() || path.isEmpty()) {
        return {};
    }
    return (host.startsWith(QLatin1String("http://")) ? host : QStringLiteral("https://") + host) + QLatin1Char('/') + path;
}

QString cloneFolderName(const QString &url)
{
    QString path = url.trimmed();
    while (path.endsWith(QLatin1Char('/'))) {
        path.chop(1);
    }
    if (path.endsWith(QLatin1String("/.git"))) {
        path.chop(5);
    }
    QString name = path.section(QLatin1Char('/'), -1);
    if (!path.contains(QLatin1String("://")) && name.contains(QLatin1Char(':'))) {
        name = name.section(QLatin1Char(':'), -1);
    }
    if (name.endsWith(QLatin1String(".git"))) {
        name.chop(4);
    }
    return name.isEmpty() ? QStringLiteral("repository") : name;
}

QString logFormat()
{
    return QStringLiteral("%H%x1f%h%x1f%an%x1f%ae%x1f%at%x1f%s");
}

QList<Commit> parseLog(const QByteArray &output)
{
    QList<Commit> commits;
    for (const QByteArray &record : output.split('\0')) {
        const QList<QByteArray> fields = record.trimmed().split(FIELD);
        if (fields.size() < 6) {
            continue;
        }
        commits.append({QString::fromUtf8(fields.at(0)), QString::fromUtf8(fields.at(1)), QString::fromUtf8(fields.at(2)),
                        QString::fromUtf8(fields.at(3)), QDateTime::fromSecsSinceEpoch(fields.at(4).toLongLong()),
                        QString::fromUtf8(fields.mid(5).join(FIELD))});
    }
    return commits;
}

QString branchFormat()
{
    return QStringLiteral("%(refname)%1f%(refname:short)%1f%(HEAD)%1f%(upstream:short)%1f%(upstream:track,nobracket)%1f%(committerdate:unix)");
}

QList<Branch> parseBranches(const QByteArray &output)
{
    QList<Branch> branches;
    for (const QByteArray &line : output.split('\n')) {
        const QList<QByteArray> fields = line.split(FIELD);
        if (fields.size() < 6) {
            continue;
        }
        const QByteArray ref = fields.at(0);
        const bool remote = ref.startsWith("refs/remotes/");
        // origin/HEAD only names the remote's default branch.
        if (remote && ref.endsWith("/HEAD")) {
            continue;
        }
        branches.append({QString::fromUtf8(fields.at(1)), remote, fields.at(2) == "*", QString::fromUtf8(fields.at(3)),
                         QString::fromUtf8(fields.at(4)), QDateTime::fromSecsSinceEpoch(fields.at(5).trimmed().toLongLong())});
    }
    std::stable_sort(branches.begin(), branches.end(), [](const Branch &a, const Branch &b) {
        if (a.remote != b.remote) {
            return !a.remote;
        }
        return a.time > b.time;
    });
    return branches;
}

} // namespace fe::git
