// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements gitrepo.h.

#include "core/gitrepo.h"

#include <QDir>
#include <QHash>
#include <QFile>
#include <QFileInfo>

#include <array>
#include <climits>

#include <sys/stat.h>
#include <unistd.h>

namespace fe::git {

namespace {

/// The most of a HEAD or gitfile that is read: a ref name or a path, never a whole file.
constexpr qint64 SMALL_FILE_LIMIT = 4096;

QByteArray readSmallFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return file.read(SMALL_FILE_LIMIT);
}

bool isHex(const QByteArray &text)
{
    for (const char c : text) {
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
            return false;
        }
    }
    return true;
}

/// git's validate_headref (setup.c): a link into refs/, "ref: refs/...", or an object id of SHA-1 or SHA-256.
bool isValidHead(const QString &path)
{
    const QFileInfo head(path);
    if (head.isSymLink()) {
        std::array<char, PATH_MAX> target{};
        const ssize_t length = ::readlink(QFile::encodeName(path).constData(), target.data(), target.size() - 1);
        return length > 0 && QByteArray(target.data(), length).startsWith("refs/");
    }
    if (!head.isFile()) {
        return false;
    }
    const QByteArray content = readSmallFile(path).trimmed();
    if (content.startsWith("ref:")) {
        return content.mid(4).trimmed().startsWith("refs/");
    }
    return (content.size() == 40 || content.size() == 64) && isHex(content);
}

/// A path a Git file names, which is relative to the folder of that file unless it is absolute.
QString resolveFrom(const QString &folder, const QString &named)
{
    return QDir::cleanPath(QDir::isAbsolutePath(named) ? named : folder + QLatin1Char('/') + named);
}

/// The common directory of gitDir (setup.c, get_common_dir): what its commondir file names, or gitDir itself.
QString commonDirOf(const QString &gitDir)
{
    const QString named = QString::fromUtf8(readSmallFile(gitDir + QStringLiteral("/commondir")).trimmed());
    return named.isEmpty() ? gitDir : resolveFrom(gitDir, named);
}

bool isSearchableFolder(const QString &path)
{
    return ::access(QFile::encodeName(path).constData(), X_OK) == 0 && QFileInfo(path).isDir();
}

/// The folder a `.git` file names (setup.c, read_gitfile_gently: "gitdir: <path>"), or nothing when it names none.
std::optional<QString> gitFileTarget(const QString &gitFile)
{
    const QByteArray content = readSmallFile(gitFile);
    if (!content.startsWith("gitdir: ")) {
        return std::nullopt;
    }
    const QString named = QString::fromUtf8(content.mid(8).trimmed());
    if (named.isEmpty()) {
        return std::nullopt;
    }
    return resolveFrom(QFileInfo(gitFile).absolutePath(), named);
}

std::optional<dev_t> deviceOf(const QString &path)
{
    struct stat info {};
    if (::stat(QFile::encodeName(path).constData(), &info) != 0) {
        return std::nullopt;
    }
    return info.st_dev;
}

/// git's is_path_owned_by_current_user (compat/mingw.c and git-compat-util.h): the effective user, or the user sudo
/// was started by when running as root.
bool ownedByCurrentUser(const QString &path)
{
    struct stat info {};
    if (::lstat(QFile::encodeName(path).constData(), &info) != 0) {
        return false;
    }
    uid_t user = ::geteuid();
    if (user == 0) {
        bool ok = false;
        const uint sudoUser = qEnvironmentVariable("SUDO_UID").toUInt(&ok);
        if (ok) {
            user = sudoUser;
        }
    }
    return info.st_uid == user;
}

/// The absolute folders of GIT_CEILING_DIRECTORIES that are real folders, without trailing slashes.
QStringList ceilingFolders()
{
    QStringList folders;
    const QStringList listed = qEnvironmentVariable("GIT_CEILING_DIRECTORIES").split(QLatin1Char(':'), Qt::SkipEmptyParts);
    for (const QString &entry : listed) {
        if (!QDir::isAbsolutePath(entry)) {
            continue;
        }
        const QString real = QFileInfo(entry).canonicalFilePath();
        folders.append(real.isEmpty() ? QDir::cleanPath(entry) : real);
    }
    return folders;
}

bool isTruthy(const QString &value)
{
    const QString lower = value.trimmed().toLower();
    return lower == u"1" || lower == u"true" || lower == u"yes" || lower == u"on";
}

} // namespace

bool isGitDirectory(const QString &path)
{
    if (!isValidHead(path + QStringLiteral("/HEAD"))) {
        return false;
    }
    const QString common = commonDirOf(path);
    return isSearchableFolder(common + QStringLiteral("/objects")) && isSearchableFolder(common + QStringLiteral("/refs"));
}

bool isSafeDirectory(const QString &workTree, const QStringList &entries)
{
    const QString target = QDir::cleanPath(workTree);
    bool safe = false;
    for (const QString &raw : entries) {
        const QString entry = raw.trimmed();
        if (entry.isEmpty()) {
            safe = false;
            continue;
        }
        if (entry == u"*") {
            safe = true;
            continue;
        }
        QString path = entry.startsWith(QLatin1String("~/")) ? QDir::homePath() + entry.mid(1) : entry;
        if (path.endsWith(QLatin1String("/*"))) {
            const QString prefix = QDir::cleanPath(path.chopped(2));
            if (target == prefix || target.startsWith(prefix + QLatin1Char('/'))) {
                safe = true;
            }
            continue;
        }
        const QString real = QFileInfo(path).canonicalFilePath();
        if (QDir::cleanPath(real.isEmpty() ? path : real) == target) {
            safe = true;
        }
    }
    return safe;
}

std::optional<Repository> repositoryOf(const QString &path, bool isFolder, const std::function<QStringList()> &safeDirectories)
{
    const QFileInfo item(path);
    // git works from the folder it is started in, with its links resolved (getcwd), so a link to a folder leads into
    // the repository of what it points at.
    QString folder = QFileInfo(isFolder ? path : item.absolutePath()).canonicalFilePath();
    if (folder.isEmpty()) {
        return std::nullopt;
    }
    const QStringList ceilings = ceilingFolders();
    const bool acrossFilesystems = isTruthy(qEnvironmentVariable("GIT_DISCOVERY_ACROSS_FILESYSTEM"));
    const std::optional<dev_t> startDevice = deviceOf(folder);

    while (true) {
        const QString dotGit = folder == u"/" ? QStringLiteral("/.git") : folder + QStringLiteral("/.git");
        const QFileInfo dotGitInfo(dotGit);
        std::optional<QString> gitDir;
        QString gitFile;
        if (dotGitInfo.isFile()) {
            // A .git file that names no Git directory stops git with "invalid gitfile format" or "not a git
            // repository"; it never looks further up.
            const std::optional<QString> named = gitFileTarget(dotGit);
            if (!named || !isGitDirectory(*named)) {
                return std::nullopt;
            }
            gitDir = QFileInfo(*named).canonicalFilePath();
            gitFile = dotGit;
        } else if (dotGitInfo.isDir() && isGitDirectory(dotGit)) {
            gitDir = QFileInfo(dotGit).canonicalFilePath();
        }
        if (gitDir) {
            const bool owned = (gitFile.isEmpty() || ownedByCurrentUser(gitFile)) && ownedByCurrentUser(folder) && ownedByCurrentUser(*gitDir);
            if (!owned && !isSafeDirectory(folder, safeDirectories ? safeDirectories() : QStringList())) {
                return std::nullopt;
            }
            return Repository{folder, *gitDir, QFileInfo(commonDirOf(*gitDir)).canonicalFilePath()};
        }
        // A folder that is a Git directory itself is a bare repository, which has no work tree.
        if (isGitDirectory(folder)) {
            return std::nullopt;
        }
        if (folder == u"/") {
            return std::nullopt;
        }
        const QString parent = QFileInfo(folder).absolutePath();
        // git never goes up into a ceiling folder, nor onto another filesystem.
        if (ceilings.contains(parent)) {
            return std::nullopt;
        }
        if (!acrossFilesystems && deviceOf(parent) != startDevice) {
            return std::nullopt;
        }
        folder = parent;
    }
}

Target targetOf(const QList<SelectedPath> &items, bool background, const std::function<QStringList()> &safeDirectories)
{
    Target target;
    if (items.isEmpty()) {
        return target;
    }
    if (background) {
        const QString folder = items.constFirst().path;
        if (const std::optional<Repository> repository = repositoryOf(folder, true, safeDirectories)) {
            target.kind = Target::Kind::Repository;
            target.repository = *repository;
        } else if (QFileInfo info(folder); info.isDir() && info.isWritable()) {
            target.kind = Target::Kind::Outside;
            target.folder = info.canonicalFilePath();
        }
        return target;
    }
    std::optional<Repository> shared;
    QStringList paths;
    // Files of one folder share its repository: the search up the tree runs once per folder, not once per file.
    QHash<QString, std::optional<Repository>> byFolder;
    for (const SelectedPath &item : items) {
        std::optional<Repository> repository;
        if (item.isFolder) {
            repository = repositoryOf(item.path, true, safeDirectories);
        } else {
            const QString folder = QFileInfo(item.path).absolutePath();
            const auto known = byFolder.constFind(folder);
            repository = known != byFolder.cend() ? *known : byFolder.insert(folder, repositoryOf(item.path, false, safeDirectories)).value();
        }
        if (!repository || (shared && shared->workTree != repository->workTree)) {
            return {};
        }
        shared = repository;
        const QFileInfo info(item.path);
        const QString real = item.isFolder ? info.canonicalFilePath() : QFileInfo(info.absolutePath()).canonicalFilePath() + QLatin1Char('/') + info.fileName();
        if (real == repository->workTree) {
            // The top of a work tree is the repository itself, which only a menu of its own can act on.
            if (items.size() > 1) {
                return {};
            }
            target.kind = Target::Kind::Repository;
            target.repository = *repository;
            return target;
        }
        paths.append(real.mid(repository->workTree.size() + (repository->workTree == u"/" ? 0 : 1)));
    }
    target.kind = Target::Kind::Items;
    target.repository = *shared;
    target.paths = paths;
    return target;
}

} // namespace fe::git
