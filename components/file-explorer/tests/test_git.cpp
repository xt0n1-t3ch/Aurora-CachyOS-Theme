// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Tests of File Explorer's Git logic without widgets: which repository holds an item, checked against git itself on
// real repositories in a temporary folder, what `git status` says, and what the Git menu offers in each state.

#include "core/gitcommands.h"
#include "core/gitmenu.h"
#include "core/gitrepo.h"
#include "core/gitstatus.h"

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <QTest>

#include <optional>

namespace {

/// The environment every git of these tests runs in: no system or user configuration, so the machine's own settings
/// cannot change a result.
QProcessEnvironment cleanGitEnvironment()
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("GIT_CONFIG_NOSYSTEM"), QStringLiteral("1"));
    environment.insert(QStringLiteral("GIT_CONFIG_GLOBAL"), QStringLiteral("/dev/null"));
    environment.insert(QStringLiteral("GIT_AUTHOR_NAME"), QStringLiteral("Test"));
    environment.insert(QStringLiteral("GIT_AUTHOR_EMAIL"), QStringLiteral("test@example.org"));
    environment.insert(QStringLiteral("GIT_COMMITTER_NAME"), QStringLiteral("Test"));
    environment.insert(QStringLiteral("GIT_COMMITTER_EMAIL"), QStringLiteral("test@example.org"));
    return environment;
}

struct GitRun {
    int exitCode = -1;
    QString output;
};

GitRun runGit(const QString &folder, const QStringList &arguments)
{
    QProcess process;
    process.setProcessEnvironment(cleanGitEnvironment());
    process.setWorkingDirectory(folder);
    process.start(QStringLiteral("git"), QStringList{QStringLiteral("-c"), QStringLiteral("init.defaultBranch=main")} + arguments);
    process.waitForFinished(30000);
    return {process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1, QString::fromUtf8(process.readAllStandardOutput()).trimmed()};
}

void writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

/// What git itself says is the top of the work tree for an item, or nothing when git refuses to work there.
std::optional<QString> gitsOwnAnswer(const QString &path, bool isFolder)
{
    const GitRun run = runGit(isFolder ? path : QFileInfo(path).absolutePath(), {QStringLiteral("rev-parse"), QStringLiteral("--show-toplevel")});
    if (run.exitCode != 0 || run.output.isEmpty()) {
        return std::nullopt;
    }
    return run.output;
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

/// What `git status` says of the work tree at folder, read as File Explorer reads it.
fe::git::Status statusOf(const QString &folder)
{
    QProcess process;
    process.setProcessEnvironment(cleanGitEnvironment());
    process.setWorkingDirectory(folder);
    process.start(QStringLiteral("git"), {QStringLiteral("status"), QStringLiteral("--porcelain=v2"), QStringLiteral("--branch"), QStringLiteral("--show-stash"), QStringLiteral("-z")});
    process.waitForFinished(30000);
    return fe::git::parseStatus(process.readAllStandardOutput()).value_or(fe::git::Status{});
}

/// A menu's entries in one line each: "|" for a line before it, "-" for a greyed entry, then its words.
QStringList described(const fe::git::Menu &menu)
{
    QStringList lines;
    for (const fe::git::MenuEntry &entry : menu.entries) {
        lines.append(QStringLiteral("%1%2%3").arg(entry.separatorBefore ? QStringLiteral("|") : QString(), entry.enabled ? QString() : QStringLiteral("-"), entry.label));
    }
    return lines;
}

} // namespace

class GitTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_root;
    QString at(const QString &relative) const { return QFileInfo(m_root.path()).canonicalFilePath() + QLatin1Char('/') + relative; }

private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_root.isValid());
        // A repository with a commit, folders in it, a nested repository, a folder with an empty .git, one with a
        // .git file that names nothing, a linked work tree, a submodule, a bare repository and a plain folder.
        writeFile(at(QStringLiteral("plain/notes.txt")), "notes\n");
        writeFile(at(QStringLiteral("repo/readme.md")), "readme\n");
        writeFile(at(QStringLiteral("repo/sub/deep/code.c")), "int x;\n");
        QCOMPARE(runGit(at(QStringLiteral("repo")), {QStringLiteral("init"), QStringLiteral("-q")}).exitCode, 0);
        QCOMPARE(runGit(at(QStringLiteral("repo")), {QStringLiteral("add"), QStringLiteral(".")}).exitCode, 0);
        QCOMPARE(runGit(at(QStringLiteral("repo")), {QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("first")}).exitCode, 0);
        writeFile(at(QStringLiteral("repo/nested/inner.txt")), "inner\n");
        QCOMPARE(runGit(at(QStringLiteral("repo/nested")), {QStringLiteral("init"), QStringLiteral("-q")}).exitCode, 0);
        QVERIFY(QDir().mkpath(at(QStringLiteral("repo/emptydotgit/.git"))));
        writeFile(at(QStringLiteral("repo/emptydotgit/a.txt")), "a\n");
        writeFile(at(QStringLiteral("repo/badgitfile/.git")), "not a gitfile\n");
        writeFile(at(QStringLiteral("repo/badgitfile/b.txt")), "b\n");
        QCOMPARE(runGit(at(QStringLiteral("repo")), {QStringLiteral("worktree"), QStringLiteral("add"), QStringLiteral("-q"), at(QStringLiteral("wt"))}).exitCode, 0);
        writeFile(at(QStringLiteral("lib/lib.c")), "int lib;\n");
        QCOMPARE(runGit(at(QStringLiteral("lib")), {QStringLiteral("init"), QStringLiteral("-q")}).exitCode, 0);
        QCOMPARE(runGit(at(QStringLiteral("lib")), {QStringLiteral("add"), QStringLiteral(".")}).exitCode, 0);
        QCOMPARE(runGit(at(QStringLiteral("lib")), {QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("lib")}).exitCode, 0);
        QCOMPARE(runGit(at(QStringLiteral("repo")), {QStringLiteral("-c"), QStringLiteral("protocol.file.allow=always"), QStringLiteral("submodule"), QStringLiteral("add"),
                                                    QStringLiteral("-q"), at(QStringLiteral("lib")), QStringLiteral("libsub")})
                     .exitCode,
                 0);
        QCOMPARE(runGit(m_root.path(), {QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("--bare"), at(QStringLiteral("bare.git"))}).exitCode, 0);
        QVERIFY(QFile::link(at(QStringLiteral("repo/sub")), at(QStringLiteral("link-to-sub"))));
    }

    void theSameRepositoryAsGitForEveryKindOfItem_data()
    {
        QTest::addColumn<QString>("relative");
        QTest::addColumn<bool>("isFolder");
        QTest::addColumn<QString>("expected");
        QTest::newRow("a repository's own folder") << "repo" << true << "repo";
        QTest::newRow("a file at its top") << "repo/readme.md" << false << "repo";
        QTest::newRow("a folder deep inside") << "repo/sub/deep" << true << "repo";
        QTest::newRow("a file deep inside") << "repo/sub/deep/code.c" << false << "repo";
        QTest::newRow("the .git folder's parent from a nested repository") << "repo/nested" << true << "repo/nested";
        QTest::newRow("a file of the nested repository") << "repo/nested/inner.txt" << false << "repo/nested";
        QTest::newRow("an empty .git folder is no repository") << "repo/emptydotgit" << true << "repo";
        QTest::newRow("a .git file that names nothing stops git") << "repo/badgitfile" << true << "";
        QTest::newRow("a linked work tree") << "wt" << true << "wt";
        QTest::newRow("a file of the linked work tree") << "wt/readme.md" << false << "wt";
        QTest::newRow("a submodule") << "repo/libsub" << true << "repo/libsub";
        QTest::newRow("a file of the submodule") << "repo/libsub/lib.c" << false << "repo/libsub";
        QTest::newRow("a bare repository has no work tree") << "bare.git" << true << "";
        QTest::newRow("a folder inside a bare repository") << "bare.git/refs" << true << "";
        QTest::newRow("a plain folder") << "plain" << true << "";
        QTest::newRow("a file of a plain folder") << "plain/notes.txt" << false << "";
        QTest::newRow("a link to a folder inside a repository") << "link-to-sub" << true << "repo";
    }

    void theSameRepositoryAsGitForEveryKindOfItem()
    {
        QFETCH(QString, relative);
        QFETCH(bool, isFolder);
        QFETCH(QString, expected);
        const QString path = at(relative);
        const std::optional<fe::git::Repository> found = fe::git::repositoryOf(path, isFolder);
        // git itself is the judge; the expected column says what that is, so a change in git shows up here first.
        const std::optional<QString> git = gitsOwnAnswer(path, isFolder);
        QCOMPARE(git.value_or(QString()), expected.isEmpty() ? QString() : at(expected));
        QCOMPARE(found ? found->workTree : QString(), git.value_or(QString()));
    }

    void aLinkedWorkTreeSharesTheMainRepositorysObjects()
    {
        const auto worktree = fe::git::repositoryOf(at(QStringLiteral("wt")), true);
        QVERIFY(worktree);
        QCOMPARE(worktree->commonDir, at(QStringLiteral("repo/.git")));
        QCOMPARE(worktree->gitDir, at(QStringLiteral("repo/.git/worktrees/wt")));
        const auto submodule = fe::git::repositoryOf(at(QStringLiteral("repo/libsub")), true);
        QVERIFY(submodule);
        QCOMPARE(submodule->gitDir, at(QStringLiteral("repo/.git/modules/libsub")));
    }

    void aCeilingFolderEndsTheSearchAsInGit()
    {
        qputenv("GIT_CEILING_DIRECTORIES", at(QStringLiteral("repo")).toUtf8());
        const auto found = fe::git::repositoryOf(at(QStringLiteral("repo/sub/deep")), true);
        const std::optional<QString> git = gitsOwnAnswer(at(QStringLiteral("repo/sub/deep")), true);
        // The repository's own folder is still found from itself: git stops only on the way up into a ceiling.
        const auto fromTop = fe::git::repositoryOf(at(QStringLiteral("repo")), true);
        qunsetenv("GIT_CEILING_DIRECTORIES");
        QVERIFY(!git);
        QVERIFY(!found);
        QVERIFY(fromTop);
    }

    void safeDirectoryEntriesFollowGitConfig()
    {
        QVERIFY(fe::git::isSafeDirectory(QStringLiteral("/srv/code"), {QStringLiteral("*")}));
        QVERIFY(fe::git::isSafeDirectory(QStringLiteral("/srv/code"), {QStringLiteral("/srv/code")}));
        QVERIFY(fe::git::isSafeDirectory(QStringLiteral("/srv/code"), {QStringLiteral("/srv/code/")}));
        QVERIFY(fe::git::isSafeDirectory(QStringLiteral("/srv/code/app"), {QStringLiteral("/srv/*")}));
        QVERIFY(!fe::git::isSafeDirectory(QStringLiteral("/srv/codex"), {QStringLiteral("/srv/code/*")}));
        QVERIFY(!fe::git::isSafeDirectory(QStringLiteral("/srv/code"), {QStringLiteral("/srv/other")}));
        // An empty entry forgets the ones before it, as git-config(1) says.
        QVERIFY(!fe::git::isSafeDirectory(QStringLiteral("/srv/code"), {QStringLiteral("*"), QString()}));
        QVERIFY(!fe::git::isSafeDirectory(QStringLiteral("/srv/code"), {}));
    }

    void aGitDirectoryNeedsHeadObjectsAndRefs()
    {
        QVERIFY(fe::git::isGitDirectory(at(QStringLiteral("repo/.git"))));
        QVERIFY(fe::git::isGitDirectory(at(QStringLiteral("bare.git"))));
        QVERIFY(!fe::git::isGitDirectory(at(QStringLiteral("repo/emptydotgit/.git"))));
        QVERIFY(!fe::git::isGitDirectory(at(QStringLiteral("plain"))));
    }

    void statusReadsTheBranchItsUpstreamTheStashAndEveryKindOfEntry()
    {
        const QByteArray output = QByteArrayLiteral("# branch.oid 0123456789abcdef0123456789abcdef01234567\0"
                                                    "# branch.head main\0# branch.upstream origin/main\0# branch.ab +2 -1\0# stash 3\0"
                                                    "1 .M N... 100644 100644 100644 aaa aaa src/a file.c\0"
                                                    "1 A. N... 000000 100644 100644 000 bbb new.txt\0"
                                                    "2 R. N... 100644 100644 100644 ccc ccc R100 renamed.txt\0old name.txt\0"
                                                    "u UU N... 100644 100644 100644 100644 d e f conflict.txt\0"
                                                    "1 .M SC.. 160000 160000 160000 g g libsub\0"
                                                    "? untracked dir/\0! build/\0");
        const std::optional<fe::git::Status> status = fe::git::parseStatus(output);
        QVERIFY(status);
        QCOMPARE(status->branch, QStringLiteral("main"));
        QCOMPARE(status->head, QStringLiteral("0123456789abcdef0123456789abcdef01234567"));
        QCOMPARE(status->upstream, QStringLiteral("origin/main"));
        QCOMPARE(status->ahead, 2);
        QCOMPARE(status->behind, 1);
        QCOMPARE(status->stashes, 3);
        QCOMPARE(status->entries.size(), 7);
        QCOMPARE(status->entries.at(0).path, QStringLiteral("src/a file.c"));
        QVERIFY(status->entries.at(0).hasUnstaged() && !status->entries.at(0).hasStaged());
        QVERIFY(status->entries.at(1).hasStaged() && !status->entries.at(1).hasUnstaged());
        QCOMPARE(status->entries.at(2).kind, fe::git::StatusEntry::Kind::Renamed);
        QCOMPARE(status->entries.at(2).path, QStringLiteral("renamed.txt"));
        QCOMPARE(status->entries.at(2).originalPath, QStringLiteral("old name.txt"));
        QCOMPARE(status->entries.at(3).kind, fe::git::StatusEntry::Kind::Conflicted);
        QCOMPARE(status->entries.at(3).path, QStringLiteral("conflict.txt"));
        QVERIFY(status->entries.at(4).submodule);
        QCOMPARE(status->entries.at(5).kind, fe::git::StatusEntry::Kind::Untracked);
        QCOMPARE(status->entries.at(5).path, QStringLiteral("untracked dir/"));
        QCOMPARE(status->entries.at(6).kind, fe::git::StatusEntry::Kind::Ignored);
        QCOMPARE(status->changeCount(), 6);
        QCOMPARE(status->conflictCount(), 1);
        QVERIFY(status->hasStaged());
    }

    void statusBeforeTheFirstCommitAndOnADetachedHead()
    {
        const auto initial = fe::git::parseStatus(QByteArrayLiteral("# branch.oid (initial)\0# branch.head main\0"));
        QVERIFY(initial);
        QVERIFY(initial->head.isEmpty());
        QCOMPARE(initial->branch, QStringLiteral("main"));
        QVERIFY(!initial->ahead);
        const auto detached = fe::git::parseStatus(QByteArrayLiteral("# branch.oid abc\0# branch.head (detached)\0"));
        QVERIFY(detached);
        QVERIFY(detached->branch.isEmpty());
        QVERIFY(!fe::git::parseStatus(QByteArrayLiteral(" M readme.md\0")));
        QVERIFY(!fe::git::parseStatus(QByteArray()));
    }

    void statusOfARealRepositoryIsRead()
    {
        const QString work = at(QStringLiteral("status"));
        writeFile(work + QStringLiteral("/kept.txt"), "one\n");
        writeFile(work + QStringLiteral("/gone.txt"), "gone\n");
        QCOMPARE(runGit(work, {QStringLiteral("init"), QStringLiteral("-q")}).exitCode, 0);
        QCOMPARE(runGit(work, {QStringLiteral("add"), QStringLiteral(".")}).exitCode, 0);
        QCOMPARE(runGit(work, {QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("one")}).exitCode, 0);
        writeFile(work + QStringLiteral("/kept.txt"), "two\n");
        writeFile(work + QStringLiteral("/staged.txt"), "new\n");
        writeFile(work + QStringLiteral("/loose/file.txt"), "loose\n");
        QCOMPARE(runGit(work, {QStringLiteral("add"), QStringLiteral("staged.txt")}).exitCode, 0);
        QCOMPARE(runGit(work, {QStringLiteral("rm"), QStringLiteral("-q"), QStringLiteral("gone.txt")}).exitCode, 0);
        QProcess process;
        process.setProcessEnvironment(cleanGitEnvironment());
        process.setWorkingDirectory(work);
        process.start(QStringLiteral("git"), {QStringLiteral("status"), QStringLiteral("--porcelain=v2"), QStringLiteral("--branch"), QStringLiteral("--show-stash"), QStringLiteral("-z")});
        QVERIFY(process.waitForFinished(30000));
        const auto status = fe::git::parseStatus(process.readAllStandardOutput());
        QVERIFY(status);
        QCOMPARE(status->branch, QStringLiteral("main"));
        QStringList seen;
        for (const fe::git::StatusEntry &entry : status->entries) {
            seen.append(QStringLiteral("%1%2 %3").arg(entry.staged, entry.unstaged, entry.path));
        }
        seen.sort();
        QCOMPARE(seen, (QStringList{QStringLiteral(".. loose/"), QStringLiteral(".M kept.txt"), QStringLiteral("A. staged.txt"), QStringLiteral("D. gone.txt")}));
    }

    void remotesAddressesAndFolderNames()
    {
        const QList<fe::git::Remote> remotes = fe::git::parseRemotes(
            "origin\thttps://github.com/a/b.git (fetch)\norigin\thttps://github.com/a/b.git (push)\nfork\tgit@gitlab.com:me/b.git (fetch)\nfork\tgit@gitlab.com:me/b.git (push)\n");
        QCOMPARE(remotes, (QList<fe::git::Remote>{{QStringLiteral("origin"), QStringLiteral("https://github.com/a/b.git")}, {QStringLiteral("fork"), QStringLiteral("git@gitlab.com:me/b.git")}}));
        QCOMPARE(fe::git::remoteWebPage(QStringLiteral("git@github.com:xt0n1-t3ch/Aurora-CachyOS-Theme.git")), QStringLiteral("https://github.com/xt0n1-t3ch/Aurora-CachyOS-Theme"));
        QCOMPARE(fe::git::remoteWebPage(QStringLiteral("ssh://git@codeberg.org:22/me/tool.git")), QStringLiteral("https://codeberg.org/me/tool"));
        QCOMPARE(fe::git::remoteWebPage(QStringLiteral("https://user:token@github.com/a/b.git")), QStringLiteral("https://github.com/a/b"));
        QCOMPARE(fe::git::remoteWebPage(QStringLiteral("https://git.example.org:8443/team/app/")), QStringLiteral("https://git.example.org:8443/team/app"));
        QCOMPARE(fe::git::remoteWebPage(QStringLiteral("http://192.168.1.5:3000/me/site.git")), QStringLiteral("http://192.168.1.5:3000/me/site"));
        QCOMPARE(fe::git::remoteWebPage(QStringLiteral("git://git.kernel.org/pub/scm/git/git.git")), QStringLiteral("https://git.kernel.org/pub/scm/git/git"));
        QVERIFY(fe::git::remoteWebPage(QStringLiteral("/home/me/repos/b.git")).isEmpty());
        QVERIFY(fe::git::remoteWebPage(QStringLiteral("file:///home/me/repos/b.git")).isEmpty());
        QCOMPARE(fe::git::cloneFolderName(QStringLiteral("https://github.com/a/Aurora.git")), QStringLiteral("Aurora"));
        QCOMPARE(fe::git::cloneFolderName(QStringLiteral("git@github.com:a/tool")), QStringLiteral("tool"));
        QCOMPARE(fe::git::cloneFolderName(QStringLiteral("host:solo.git")), QStringLiteral("solo"));
        QCOMPARE(fe::git::cloneFolderName(QStringLiteral("/srv/repos/site/.git/")), QStringLiteral("site"));
    }

    void logAndBranchesOfARealRepositoryAreRead()
    {
        const QString repo = at(QStringLiteral("repo"));
        QCOMPARE(runGit(repo, {QStringLiteral("branch"), QStringLiteral("feature")}).exitCode, 0);
        QProcess log;
        log.setProcessEnvironment(cleanGitEnvironment());
        log.setWorkingDirectory(repo);
        log.start(QStringLiteral("git"), {QStringLiteral("log"), QStringLiteral("-z"), QStringLiteral("--format=") + fe::git::logFormat()});
        QVERIFY(log.waitForFinished(30000));
        const QList<fe::git::Commit> commits = fe::git::parseLog(log.readAllStandardOutput());
        QVERIFY(!commits.isEmpty());
        QCOMPARE(commits.last().subject, QStringLiteral("first"));
        QCOMPARE(commits.last().author, QStringLiteral("Test"));
        QCOMPARE(commits.last().hash.size(), 40);
        QVERIFY(commits.last().hash.startsWith(commits.last().shortHash));
        QProcess refs;
        refs.setProcessEnvironment(cleanGitEnvironment());
        refs.setWorkingDirectory(repo);
        refs.start(QStringLiteral("git"), {QStringLiteral("for-each-ref"), QStringLiteral("--format=") + fe::git::branchFormat(), QStringLiteral("refs/heads"), QStringLiteral("refs/remotes")});
        QVERIFY(refs.waitForFinished(30000));
        const QList<fe::git::Branch> branches = fe::git::parseBranches(refs.readAllStandardOutput());
        QStringList names;
        for (const fe::git::Branch &branch : branches) {
            names.append(branch.current ? branch.name + QStringLiteral(" *") : branch.name);
        }
        names.sort();
        QVERIFY(names.contains(QStringLiteral("main *")));
        QVERIFY(names.contains(QStringLiteral("feature")));
    }

    void anOperationUnderWayIsReadFromTheGitDirectory()
    {
        QTemporaryDir gitDir;
        QCOMPARE(fe::git::operationInProgress(gitDir.path()), fe::git::Operation::None);
        writeFile(gitDir.filePath(QStringLiteral("MERGE_HEAD")), "abc\n");
        QCOMPARE(fe::git::operationInProgress(gitDir.path()), fe::git::Operation::Merge);
        QVERIFY(QDir().mkpath(gitDir.filePath(QStringLiteral("rebase-merge"))));
        QCOMPARE(fe::git::operationInProgress(gitDir.path()), fe::git::Operation::Rebase);
    }

    void theMenuSaysWhereTheRepositoryStands_data()
    {
        QTest::addColumn<QByteArray>("status");
        QTest::addColumn<bool>("remote");
        QTest::addColumn<int>("operation");
        QTest::addColumn<QString>("line");
        QTest::addColumn<QStringList>("entries");
        const QByteArray head = QByteArrayLiteral("# branch.oid 1234567890abcdef1234567890abcdef12345678\0# branch.head main\0");
        const QByteArray tracking = QByteArrayLiteral("# branch.upstream origin/main\0");
        const QByteArray changes = QByteArrayLiteral("1 .M N... 100644 100644 100644 a a one.txt\0? two.txt\0? three.txt\0");
        QTest::newRow("up to date") << (head + tracking + QByteArrayLiteral("# branch.ab +0 -0\0")) << true << 0 << "main, up to date with origin/main"
                                    << QStringList{"-Nothing to Commit", "|Pull", "-Nothing to Push", "Fetch", "|Branches…", "-Nothing to Stash", "|-Show Changes…",
                                                   "History…", "|Open on github.com"};
        QTest::newRow("work to share both ways") << (head + tracking + QByteArrayLiteral("# branch.ab +2 -1\0# stash 2\0") + changes) << true << 0
                                                 << "main, 2 to push, 1 to pull"
                                                 << QStringList{"Commit 3 Changes…", "|Pull 1 Commit", "Push 2 Commits", "Fetch", "|Branches…", "Stash 3 Changes",
                                                                "Restore Latest Stash (2 saved)", "|Show Changes…", "History…", "|Open on github.com"};
        QTest::newRow("a branch never pushed") << head << true << 0 << "main, not published"
                                               << QStringList{"-Nothing to Commit", "|Publish Branch to origin", "Fetch", "|Branches…", "-Nothing to Stash",
                                                              "|-Show Changes…", "History…", "|Open on github.com"};
        QTest::newRow("no remote at all") << (head + changes) << false << 0 << "main, no remote"
                                          << QStringList{"Commit 3 Changes…", "|Branches…", "Stash 3 Changes", "|Show Changes…", "History…"};
        QTest::newRow("no commits yet") << (QByteArrayLiteral("# branch.oid (initial)\0# branch.head main\0") + changes) << false << 0
                                        << "main, no commits yet" << QStringList{"Commit 3 Changes…", "|-Branches…", "|Show Changes…", "-History…"};
        QTest::newRow("detached") << QByteArrayLiteral("# branch.oid 1234567890abcdef1234567890abcdef12345678\0# branch.head (detached)\0") << true << 0
                                  << "Detached at 1234567"
                                  << QStringList{"-Nothing to Commit", "|Switch to a Branch…", "-Nothing to Stash", "|-Show Changes…", "History…",
                                                 "|Open on github.com"};
        QTest::newRow("merge with conflicts")
            << (head + tracking + QByteArrayLiteral("# branch.ab +1 -0\0u UU N... 100644 100644 100644 100644 a b c x.txt\0u AA N... 0 100644 100644 100644 a b c y.txt\0"))
            << true << int(fe::git::Operation::Merge) << "Merging into main, 2 conflicts"
            << QStringList{"-Resolve 2 Conflicts First", "|Pull", "Push 1 Commit", "Fetch", "|Branches…", "|Show Changes…", "History…", "|Abort Merge",
                           "|Open on github.com"};
        QTest::newRow("rebase ready to go on") << (head + tracking + QByteArrayLiteral("# branch.ab +0 -0\0")) << true << int(fe::git::Operation::Rebase)
                                               << "Rebasing main"
                                               << QStringList{"-Nothing to Commit", "|Pull", "-Nothing to Push", "Fetch", "|Branches…", "|-Show Changes…",
                                                              "History…", "|Abort Rebase", "Continue Rebase", "|Open on github.com"};
    }

    void theMenuSaysWhereTheRepositoryStands()
    {
        QFETCH(QByteArray, status);
        QFETCH(bool, remote);
        QFETCH(int, operation);
        QFETCH(QString, line);
        QFETCH(QStringList, entries);
        fe::git::RepositoryState state{*fe::git::parseStatus(status), {}, fe::git::Operation(operation)};
        if (remote) {
            state.remotes = {{QStringLiteral("origin"), QStringLiteral("git@github.com:a/b.git")}};
        }
        const fe::git::Menu menu = fe::git::repositoryMenu(state);
        QCOMPARE(menu.header, line);
        QCOMPARE(described(menu), entries);
    }

    void itemsGetWhatTheirStateAllows()
    {
        const QByteArray head = QByteArrayLiteral("# branch.oid 1234567890abcdef1234567890abcdef12345678\0# branch.head main\0");
        const fe::git::RepositoryState state{
            *fe::git::parseStatus(head + QByteArrayLiteral("1 .M N... 100644 100644 100644 a a src/edit.c\0"
                                                          "1 M. N... 100644 100644 100644 a b src/staged.c\0"
                                                          "? notes/\0"
                                                          "u UU N... 100644 100644 100644 100644 a b c clash.txt\0")),
            {},
            fe::git::Operation::None};
        const auto itemEntries = [&state](const QList<fe::git::ItemFacts> &items) {
            QStringList found = described(fe::git::itemsMenu(state, items));
            return found.mid(0, 7);
        };
        QCOMPARE(itemEntries({{QStringLiteral("src/edit.c"), true, false}}),
                 (QStringList{"Stage", "-Unstage", "Discard Changes…", "|Show Changes…", "History…", "|Stop Tracking", "-Ignore"}));
        QCOMPARE(itemEntries({{QStringLiteral("src/staged.c"), true, false}}),
                 (QStringList{"-Stage", "Unstage", "Discard Changes…", "|Show Changes…", "History…", "|Stop Tracking", "-Ignore"}));
        // A folder takes the state of what is inside it.
        QCOMPARE(itemEntries({{QStringLiteral("src"), true, false}}),
                 (QStringList{"Stage", "Unstage", "Discard Changes…", "|Show Changes…", "History…", "|Stop Tracking", "-Ignore"}));
        // A file inside an untracked folder is untracked too.
        QCOMPARE(itemEntries({{QStringLiteral("notes/todo.md"), false, false}}),
                 (QStringList{"Stage", "-Unstage", "-Discard Changes…", "|Show Changes…", "-History…", "|-Stop Tracking", "Ignore"}));
        QCOMPARE(itemEntries({{QStringLiteral("clash.txt"), true, false}}),
                 (QStringList{"Mark as Resolved", "-Unstage", "-Discard Changes…", "|Show Changes…", "History…", "|Stop Tracking", "-Ignore"}));
        QCOMPARE(itemEntries({{QStringLiteral("readme.md"), true, false}}),
                 (QStringList{"-Stage", "-Unstage", "-Discard Changes…", "|-Show Changes…", "History…", "|Stop Tracking", "-Ignore"}));
        QCOMPARE(itemEntries({{QStringLiteral("build"), false, true}}),
                 (QStringList{"-Stage", "-Unstage", "-Discard Changes…", "|-Show Changes…", "-History…", "|-Stop Tracking", "-Ignore"}));
        // The repository's commands follow the item's own, and no two entries read the same.
        QStringList whole = described(fe::git::itemsMenu(state, {{QStringLiteral("src/edit.c"), true, false}}));
        QVERIFY(whole.contains(QStringLiteral("|-Resolve 1 Conflict First")));
        QVERIFY(whole.contains(QStringLiteral("|Show All Changes\u2026")));
        QVERIFY(whole.contains(QStringLiteral("Repository History\u2026")));
        for (QString &line : whole) {
            line.remove(QLatin1Char('|')).remove(QLatin1Char('-'));
        }
        QCOMPARE(whole.removeDuplicates(), 0);
    }


    void theMenuIsForWhatTheSelectionIs()
    {
        using Kind = fe::git::Target::Kind;
        const auto targetFor = [this](const QList<QPair<QString, bool>> &items, bool background) {
            QList<fe::git::SelectedPath> selected;
            for (const auto &[relative, folder] : items) {
                selected.append({at(relative), folder});
            }
            return fe::git::targetOf(selected, background);
        };
        const fe::git::Target outside = targetFor({{QStringLiteral("plain"), true}}, true);
        QCOMPARE(outside.kind, Kind::Outside);
        QCOMPARE(outside.folder, at(QStringLiteral("plain")));
        const fe::git::Target insideBackground = targetFor({{QStringLiteral("repo/sub"), true}}, true);
        QCOMPARE(insideBackground.kind, Kind::Repository);
        QCOMPARE(insideBackground.repository.workTree, at(QStringLiteral("repo")));
        // A repository's folder seen from a folder that is no repository: its own menu, never Clone.
        const fe::git::Target repoFolder = targetFor({{QStringLiteral("repo"), true}}, false);
        QCOMPARE(repoFolder.kind, Kind::Repository);
        QCOMPARE(repoFolder.repository.workTree, at(QStringLiteral("repo")));
        QCOMPARE(targetFor({{QStringLiteral("repo/libsub"), true}}, false).repository.workTree, at(QStringLiteral("repo/libsub")));
        QCOMPARE(targetFor({{QStringLiteral("plain"), true}}, false).kind, Kind::None);
        QCOMPARE(targetFor({{QStringLiteral("plain/notes.txt"), false}}, false).kind, Kind::None);
        const fe::git::Target items = targetFor({{QStringLiteral("repo/readme.md"), false}, {QStringLiteral("repo/sub/deep"), true}}, false);
        QCOMPARE(items.kind, Kind::Items);
        QCOMPARE(items.paths, (QStringList{QStringLiteral("readme.md"), QStringLiteral("sub/deep")}));
        QCOMPARE(targetFor({{QStringLiteral("link-to-sub"), true}}, false).paths, QStringList{QStringLiteral("sub")});
        QCOMPARE(targetFor({{QStringLiteral("repo/readme.md"), false}, {QStringLiteral("repo/nested/inner.txt"), false}}, false).kind, Kind::None);
        QCOMPARE(targetFor({{QStringLiteral("plain"), true}, {QStringLiteral("repo"), true}}, false).kind, Kind::None);
        QCOMPARE(targetFor({{QStringLiteral("repo"), true}, {QStringLiteral("repo/readme.md"), false}}, false).kind, Kind::None);
        QCOMPARE(targetFor({}, false).kind, Kind::None);
    }


    void discardPutsBackTrackedFilesAndNeverDeletesNewOnes()
    {
        const QString work = at(QStringLiteral("discard"));
        writeFile(work + QStringLiteral("/kept.txt"), "committed\n");
        QCOMPARE(runGit(work, {QStringLiteral("init"), QStringLiteral("-q")}).exitCode, 0);
        QCOMPARE(runGit(work, {QStringLiteral("add"), QStringLiteral(".")}).exitCode, 0);
        QCOMPARE(runGit(work, {QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("one")}).exitCode, 0);
        writeFile(work + QStringLiteral("/kept.txt"), "edited\n");
        writeFile(work + QStringLiteral("/brand new.txt"), "mine\n");
        QCOMPARE(runGit(work, {QStringLiteral("add"), QStringLiteral("brand new.txt")}).exitCode, 0);
        const fe::git::Status status = statusOf(work);
        for (const QStringList &step : fe::git::discardSteps(status.entries)) {
            QCOMPARE(runGit(work, step).exitCode, 0);
        }
        QCOMPARE(readFile(work + QStringLiteral("/kept.txt")), QByteArray("committed\n"));
        QCOMPARE(readFile(work + QStringLiteral("/brand new.txt")), QByteArray("mine\n"));
        const fe::git::Status after = statusOf(work);
        QCOMPARE(after.entries.size(), 1);
        QCOMPARE(after.entries.constFirst().kind, fe::git::StatusEntry::Kind::Untracked);
    }

    void aCommitTakesOnlyTheChosenChangesAndRenamesWhole()
    {
        const QString work = at(QStringLiteral("commit"));
        writeFile(work + QStringLiteral("/a.txt"), "a\n");
        writeFile(work + QStringLiteral("/b.txt"), "b\n");
        writeFile(work + QStringLiteral("/old.txt"), "old\n");
        QCOMPARE(runGit(work, {QStringLiteral("init"), QStringLiteral("-q")}).exitCode, 0);
        QCOMPARE(runGit(work, {QStringLiteral("add"), QStringLiteral(".")}).exitCode, 0);
        QCOMPARE(runGit(work, {QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("one")}).exitCode, 0);
        writeFile(work + QStringLiteral("/a.txt"), "a2\n");
        writeFile(work + QStringLiteral("/b.txt"), "b2\n");
        QCOMPARE(runGit(work, {QStringLiteral("mv"), QStringLiteral("old.txt"), QStringLiteral("new.txt")}).exitCode, 0);
        QList<fe::git::StatusEntry> chosen;
        for (const fe::git::StatusEntry &entry : statusOf(work).entries) {
            if (entry.path != u"b.txt") {
                chosen.append(entry);
            }
        }
        for (const QStringList &step : fe::git::commitSteps(chosen, QStringLiteral("  Two  "), QStringLiteral("Body line."), false, false)) {
            QCOMPARE(runGit(work, step).exitCode, 0);
        }
        QCOMPARE(runGit(work, {QStringLiteral("log"), QStringLiteral("-1"), QStringLiteral("--format=%s|%b")}).output, QStringLiteral("Two|Body line."));
        QCOMPARE(runGit(work, {QStringLiteral("show"), QStringLiteral("--name-status"), QStringLiteral("--format="), QStringLiteral("-M")}).output,
                 QStringLiteral("M\ta.txt\nR100\told.txt\tnew.txt"));
        // b.txt was not chosen: it is still changed and not committed.
        const fe::git::Status after = statusOf(work);
        QCOMPARE(after.entries.size(), 1);
        QCOMPARE(after.entries.constFirst().path, QStringLiteral("b.txt"));
        // Amending with nothing chosen changes only the message.
        for (const QStringList &step : fe::git::commitSteps({}, QStringLiteral("Two, reworded"), QString(), true, false)) {
            QCOMPARE(runGit(work, step).exitCode, 0);
        }
        QCOMPARE(runGit(work, {QStringLiteral("log"), QStringLiteral("--format=%s")}).output, QStringLiteral("Two, reworded\none"));
    }

    void unstagingBeforeTheFirstCommitKeepsTheFile()
    {
        const QString work = at(QStringLiteral("unstage"));
        writeFile(work + QStringLiteral("/f.txt"), "f\n");
        QCOMPARE(runGit(work, {QStringLiteral("init"), QStringLiteral("-q")}).exitCode, 0);
        QCOMPARE(runGit(work, fe::git::stageArguments({QStringLiteral("f.txt")})).exitCode, 0);
        QVERIFY(statusOf(work).hasStaged());
        QCOMPARE(runGit(work, fe::git::unstageArguments({QStringLiteral("f.txt")}, false)).exitCode, 0);
        QCOMPARE(statusOf(work).entries.constFirst().kind, fe::git::StatusEntry::Kind::Untracked);
        QVERIFY(QFile::exists(work + QStringLiteral("/f.txt")));
    }

    void anIgnoreLineMatchesThatPathAndNothingElse()
    {
        const QString work = at(QStringLiteral("ignore"));
        QCOMPARE(runGit(work.left(work.lastIndexOf(QLatin1Char('/'))), {QStringLiteral("init"), QStringLiteral("-q"), work}).exitCode, 0);
        const QStringList names{QStringLiteral("a*b.txt"), QStringLiteral("#notes"), QStringLiteral("!keep"), QStringLiteral("trail  "), QStringLiteral("sub/x[1].log")};
        QByteArray rules;
        for (const QString &name : names) {
            writeFile(work + QLatin1Char('/') + name, "x\n");
            rules += fe::git::ignoreLine(name, false).toUtf8() + '\n';
        }
        writeFile(work + QStringLiteral("/aXb.txt"), "x\n");
        writeFile(work + QStringLiteral("/sub/x1.log"), "x\n");
        QVERIFY(QDir().mkpath(work + QStringLiteral("/build")));
        writeFile(work + QStringLiteral("/build/out.o"), "x\n");
        rules += fe::git::ignoreLine(QStringLiteral("build"), true).toUtf8() + '\n';
        writeFile(work + QStringLiteral("/.gitignore"), rules);
        const GitRun ignored = runGit(work, QStringList{QStringLiteral("check-ignore"), QStringLiteral("--")} + names +
                                                QStringList{QStringLiteral("aXb.txt"), QStringLiteral("sub/x1.log"), QStringLiteral("build/out.o")});
        QCOMPARE(ignored.output.split(QLatin1Char('\n')), names + QStringList{QStringLiteral("build/out.o")});
        QCOMPARE(fe::git::ignoreLine(QStringLiteral("build"), true), QStringLiteral("/build/"));
    }

    void branchNamesFollowGitsOwnRules_data()
    {
        QTest::addColumn<QString>("name");
        for (const char *name : {"feature/login", "fix-1.2", "bad name", "-x", "a..b", "x.lock", "x/", ".hidden", "a/.b", "x@{1}", "a:b", "a~1", "tab\tname", "@", "HEAD",
                                 "ends.", "two//slashes", "what?", "star*"}) {
            QTest::newRow(name) << QString::fromUtf8(name);
        }
    }

    void branchNamesFollowGitsOwnRules()
    {
        QFETCH(QString, name);
        // git itself is the judge of what a branch may be called: it makes the branch or refuses to.
        const QString work = at(QStringLiteral("branches"));
        if (!QFile::exists(work)) {
            writeFile(work + QStringLiteral("/f.txt"), "f\n");
            QCOMPARE(runGit(work, {QStringLiteral("init"), QStringLiteral("-q")}).exitCode, 0);
            QCOMPARE(runGit(work, {QStringLiteral("add"), QStringLiteral(".")}).exitCode, 0);
            QCOMPARE(runGit(work, {QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("f")}).exitCode, 0);
        }
        const bool gitTakesIt = runGit(work, {QStringLiteral("branch"), name}).exitCode == 0;
        if (gitTakesIt) {
            QCOMPARE(runGit(work, {QStringLiteral("branch"), QStringLiteral("-D"), name}).exitCode, 0);
        }
        QCOMPARE(fe::git::branchNameProblem(name).isEmpty(), gitTakesIt);
    }

    void anUntrackedFileShowsWholeAsAdded()
    {
        const QString work = at(QStringLiteral("diff"));
        writeFile(work + QStringLiteral("/fresh.txt"), "hello\n");
        QCOMPARE(runGit(work.left(work.lastIndexOf(QLatin1Char('/'))), {QStringLiteral("init"), QStringLiteral("-q"), work}).exitCode, 0);
        const fe::git::StatusEntry entry = statusOf(work).entries.constFirst();
        const GitRun diff = runGit(work, fe::git::diffArguments(entry, false));
        QVERIFY(diff.output.contains(QStringLiteral("+hello")));
        QVERIFY(diff.output.contains(QStringLiteral("new file mode")));
        QCOMPARE(fe::git::logArguments({QStringLiteral("a.txt")}, 50).contains(QStringLiteral("--follow")), true);
        QCOMPARE(fe::git::logArguments({}, 50).contains(QStringLiteral("--follow")), false);
    }

    void aFolderOutsideRepositoriesOffersToCloneOrCreateOne()
    {
        const fe::git::Menu menu = fe::git::outsideMenu();
        QVERIFY(menu.header.isEmpty());
        QCOMPARE(described(menu), (QStringList{"Clone Repository Here…", "Create Repository Here"}));
    }
};

QTEST_GUILESS_MAIN(GitTest)
#include "test_git.moc"
