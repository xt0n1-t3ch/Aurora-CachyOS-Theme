// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Tests of File Explorer's Git menu and sheets on the offscreen platform, against real repositories and a local
// remote in a temporary folder: what the menu offers where, and what Commit, Clone, Branches, History, Stage and
// Create Repository do.

#include "core/gitrepo.h"
#include "gitactions.h"
#include "gitsheets.h"
#include "gitviews.h"

#include <QPointer>

#include <KIO/JobTracker>
#include <KJobTrackerInterface>

#include <QCheckBox>
#include <QLineEdit>
#include <QMenu>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTreeWidget>

namespace {

/// How long a test waits for git, in milliseconds: a clone or a push to a local folder takes well under a second.
constexpr int GIT_WAIT_MS = 20000;

QString gitIn(const QString &folder, const QStringList &arguments, int *exitCode = nullptr)
{
    QProcess process;
    process.setProcessEnvironment(fe::gitEnvironment());
    process.setWorkingDirectory(folder);
    process.start(QStringLiteral("git"), arguments);
    process.waitForFinished(GIT_WAIT_MS);
    if (exitCode) {
        *exitCode = process.exitCode();
    }
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

void writeFile(const QString &path, const QByteArray &content)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(content);
}

/// A plain folder that holds a repository with one commit, which follows a branch of a bare remote beside it.
struct Fixture {
    QTemporaryDir root;
    QString outer;
    QString repo;
    QString remote;

    Fixture()
    {
        const QString base = QFileInfo(root.path()).canonicalFilePath();
        outer = base + QStringLiteral("/outer");
        repo = outer + QStringLiteral("/project");
        remote = base + QStringLiteral("/origin.git");
        writeFile(repo + QStringLiteral("/a.txt"), "a\n");
        writeFile(repo + QStringLiteral("/b.txt"), "b\n");
        writeFile(outer + QStringLiteral("/notes.txt"), "notes\n");
        gitIn(base, {QStringLiteral("init"), QStringLiteral("-q"), QStringLiteral("--bare"), remote});
        gitIn(repo, {QStringLiteral("init"), QStringLiteral("-q")});
        gitIn(repo, {QStringLiteral("add"), QStringLiteral(".")});
        gitIn(repo, {QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("first")});
        gitIn(repo, {QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("origin"), remote});
        gitIn(repo, {QStringLiteral("push"), QStringLiteral("-q"), QStringLiteral("-u"), QStringLiteral("origin"), QStringLiteral("main")});
    }
};

/// The texts of a menu's actions: "-" before a greyed one, "|" for a separator.
QStringList texts(const QMenu *menu)
{
    QStringList found;
    for (const QAction *action : menu->actions()) {
        found.append(action->isSeparator() ? QStringLiteral("|") : (action->isEnabled() ? QString() : QStringLiteral("-")) + action->text());
    }
    return found;
}

/// The snapshot loadGitSnapshot reads, waited for.
std::optional<fe::GitSnapshot> snapshotOf(const QString &repo, const QStringList &paths = {})
{
    const auto repository = fe::git::repositoryOf(repo, true);
    std::optional<fe::GitSnapshot> found;
    bool done = false;
    QObject context;
    fe::loadGitSnapshot(*repository, paths, &context, [&](const std::optional<fe::GitSnapshot> &snapshot) {
        found = snapshot;
        done = true;
    });
    if (!QTest::qWaitFor([&done] { return done; }, GIT_WAIT_MS)) {
        return std::nullopt;
    }
    return found;
}

} // namespace

class GitWidgetTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // Jobs are shown by a tracker that does nothing, so no test reaches the session's notifications.
        KIO::setJobTracker(new KJobTrackerInterface(qApp));
        // git reads none of this machine's settings, and its first branch is main.
        qputenv("GIT_CONFIG_NOSYSTEM", "1");
        qputenv("GIT_CONFIG_GLOBAL", "/dev/null");
        qputenv("GIT_CONFIG_COUNT", "1");
        qputenv("GIT_CONFIG_KEY_0", "init.defaultBranch");
        qputenv("GIT_CONFIG_VALUE_0", "main");
        qputenv("GIT_AUTHOR_NAME", "Test");
        qputenv("GIT_AUTHOR_EMAIL", "test@example.org");
        qputenv("GIT_COMMITTER_NAME", "Test");
        qputenv("GIT_COMMITTER_EMAIL", "test@example.org");
    }

    void aRepositoryFolderSeenFromOutsideGetsItsOwnGitMenu()
    {
        Fixture fixture;
        writeFile(fixture.repo + QStringLiteral("/a.txt"), "changed\n");
        QWidget window;
        QMenu menu;
        const fe::GitMenuContext context{fe::git::targetOf({{fixture.repo, true}}, false), &window, {}};
        QMenu *git = fe::addGitMenu(menu, context);
        QVERIFY(git);
        QCOMPARE(git->title(), QStringLiteral("Git"));
        Q_EMIT git->aboutToShow();
        QTRY_VERIFY_WITH_TIMEOUT(texts(git).size() > 2, GIT_WAIT_MS);
        const QStringList shown = texts(git);
        QCOMPARE(shown.first(), QStringLiteral("-main, up to date with origin/main"));
        QVERIFY(shown.contains(QStringLiteral("Commit 1 Change\u2026")));
        QVERIFY(shown.contains(QStringLiteral("Pull")));
        QVERIFY(shown.contains(QStringLiteral("-Nothing to Push")));
        QVERIFY(!shown.join(QLatin1Char(' ')).contains(QStringLiteral("Clone")));
    }

    void aFolderOutsideRepositoriesGetsNoGitMenuButItsBackgroundOffersCloneAndCreate()
    {
        Fixture fixture;
        QWidget window;
        QMenu itemMenu;
        QVERIFY(!fe::addGitMenu(itemMenu, {fe::git::targetOf({{fixture.outer + QStringLiteral("/notes.txt"), false}}, false), &window, {}}));
        QVERIFY(itemMenu.actions().isEmpty());
        QMenu background;
        bool changed = false;
        const fe::GitMenuContext context{fe::git::targetOf({{fixture.outer, true}}, true), &window, [&changed] { changed = true; }};
        QMenu *git = fe::addGitMenu(background, context);
        QVERIFY(git);
        QCOMPARE(texts(git), (QStringList{QStringLiteral("Clone Repository Here\u2026"), QStringLiteral("Create Repository Here")}));
        fe::runGitCommand(fe::git::Command::CreateRepository, context, {});
        QTRY_VERIFY_WITH_TIMEOUT(changed, GIT_WAIT_MS);
        QVERIFY(fe::git::isGitDirectory(fixture.outer + QStringLiteral("/.git")));
    }

    void theCommitSheetCommitsTheChosenFilesAndPushesThem()
    {
        Fixture fixture;
        writeFile(fixture.repo + QStringLiteral("/a.txt"), "a2\n");
        writeFile(fixture.repo + QStringLiteral("/b.txt"), "b2\n");
        writeFile(fixture.repo + QStringLiteral("/c.txt"), "new\n");
        const std::optional<fe::GitSnapshot> snapshot = snapshotOf(fixture.repo);
        QVERIFY(snapshot);
        QWidget window;
        bool changed = false;
        QPointer<fe::CommitSheet> sheet = new fe::CommitSheet(*fe::git::repositoryOf(fixture.repo, true), *snapshot, [&changed] { changed = true; }, &window);
        QCOMPARE(sheet->changes()->entries().size(), 3);
        // The first change's diff shows as soon as the sheet opens.
        QTRY_VERIFY_WITH_TIMEOUT(sheet->diff()->toPlainText().contains(QStringLiteral("diff --git")), GIT_WAIT_MS);
        QVERIFY(!sheet->commitButton()->isEnabled());
        // b.txt stays out of the commit.
        for (int i = 0; i < sheet->changes()->topLevelItemCount(); ++i) {
            QTreeWidgetItem *row = sheet->changes()->topLevelItem(i);
            if (row->text(1) == u"b.txt") {
                row->setCheckState(0, Qt::Unchecked);
            }
        }
        sheet->summary()->setText(QStringLiteral("Second"));
        QVERIFY(sheet->commitAndPushButton()->isVisible() || !sheet->isVisible());
        QVERIFY(sheet->commitAndPushButton()->isEnabled());
        sheet->commitAndPushButton()->click();
        QTRY_VERIFY_WITH_TIMEOUT(changed, GIT_WAIT_MS);
        QTRY_VERIFY_WITH_TIMEOUT(!sheet || !sheet->isVisible(), GIT_WAIT_MS);
        QCOMPARE(gitIn(fixture.repo, {QStringLiteral("log"), QStringLiteral("-1"), QStringLiteral("--format=%s")}), QStringLiteral("Second"));
        QCOMPARE(gitIn(fixture.repo, {QStringLiteral("show"), QStringLiteral("--name-only"), QStringLiteral("--format=")}), QStringLiteral("a.txt\nc.txt"));
        QCOMPARE(gitIn(fixture.remote, {QStringLiteral("log"), QStringLiteral("-1"), QStringLiteral("--format=%s"), QStringLiteral("main")}), QStringLiteral("Second"));
        QCOMPARE(gitIn(fixture.repo, {QStringLiteral("status"), QStringLiteral("--short")}), QStringLiteral("M b.txt"));
    }

    void theCloneSheetClonesIntoTheFolder()
    {
        Fixture fixture;
        QWidget window;
        bool changed = false;
        auto *sheet = new fe::CloneSheet(fixture.outer, [&changed] { changed = true; }, &window);
        sheet->address()->setText(fixture.remote);
        // The folder is named after the address until it is edited.
        QCOMPARE(sheet->name()->text(), QStringLiteral("origin"));
        QVERIFY(sheet->cloneButton()->isEnabled());
        sheet->name()->setText(QStringLiteral("project"));
        Q_EMIT sheet->name()->textEdited(QStringLiteral("project"));
        QVERIFY(!sheet->cloneButton()->isEnabled());
        sheet->name()->setText(QStringLiteral("copy"));
        Q_EMIT sheet->name()->textEdited(QStringLiteral("copy"));
        QVERIFY(sheet->cloneButton()->isEnabled());
        sheet->cloneButton()->click();
        QTRY_VERIFY_WITH_TIMEOUT(changed, GIT_WAIT_MS);
        QVERIFY(QFile::exists(fixture.outer + QStringLiteral("/copy/a.txt")));
        QCOMPARE(fe::git::repositoryOf(fixture.outer + QStringLiteral("/copy"), true)->workTree, fixture.outer + QStringLiteral("/copy"));
    }

    void theBranchesSheetCreatesABranchAndSwitchesToIt()
    {
        Fixture fixture;
        QWidget window;
        auto *sheet = new fe::BranchesSheet(*fe::git::repositoryOf(fixture.repo, true), {}, &window);
        QSignalSpy listed(sheet, &fe::BranchesSheet::finished);
        QVERIFY(listed.wait(GIT_WAIT_MS));
        QVERIFY(sheet->list()->topLevelItemCount() >= 2);
        sheet->newName()->setText(QStringLiteral("bad name"));
        QVERIFY(!sheet->createButton()->isEnabled());
        sheet->newName()->setText(QStringLiteral("main"));
        QVERIFY(!sheet->createButton()->isEnabled());
        sheet->newName()->setText(QStringLiteral("feature/login"));
        QVERIFY(sheet->createButton()->isEnabled());
        sheet->createButton()->click();
        QVERIFY(listed.wait(GIT_WAIT_MS));
        QCOMPARE(gitIn(fixture.repo, {QStringLiteral("branch"), QStringLiteral("--show-current")}), QStringLiteral("feature/login"));
        // Back to main with Switch.
        for (int i = 0; i < sheet->list()->topLevelItemCount(); ++i) {
            if (sheet->list()->topLevelItem(i)->text(0) == u"main") {
                sheet->list()->setCurrentItem(sheet->list()->topLevelItem(i));
            }
        }
        QVERIFY(sheet->switchButton()->isEnabled());
        sheet->switchButton()->click();
        QVERIFY(listed.wait(GIT_WAIT_MS));
        QCOMPARE(gitIn(fixture.repo, {QStringLiteral("branch"), QStringLiteral("--show-current")}), QStringLiteral("main"));
    }

    void theHistorySheetListsTheCommitsAndShowsOne()
    {
        Fixture fixture;
        QWidget window;
        auto *sheet = new fe::HistorySheet(*fe::git::repositoryOf(fixture.repo, true), {QStringLiteral("a.txt")}, &window);
        QSignalSpy loaded(sheet, &fe::HistorySheet::loaded);
        QVERIFY(loaded.wait(GIT_WAIT_MS));
        QCOMPARE(sheet->list()->topLevelItemCount(), 1);
        QCOMPARE(sheet->list()->topLevelItem(0)->text(0), QStringLiteral("first"));
        QTRY_VERIFY_WITH_TIMEOUT(sheet->diff()->toPlainText().contains(QStringLiteral("+a")), GIT_WAIT_MS);
    }

    void stagingFromTheItemsMenuStagesOnlyThoseItems()
    {
        Fixture fixture;
        writeFile(fixture.repo + QStringLiteral("/a.txt"), "a2\n");
        writeFile(fixture.repo + QStringLiteral("/b.txt"), "b2\n");
        QWidget window;
        bool changed = false;
        const fe::GitMenuContext context{fe::git::targetOf({{fixture.repo + QStringLiteral("/a.txt"), false}}, false), &window, [&changed] { changed = true; }};
        QCOMPARE(context.target.kind, fe::git::Target::Kind::Items);
        QMenu menu;
        QMenu *git = fe::addGitMenu(menu, context);
        Q_EMIT git->aboutToShow();
        QTRY_VERIFY_WITH_TIMEOUT(texts(git).contains(QStringLiteral("Stage")), GIT_WAIT_MS);
        const auto snapshot = snapshotOf(fixture.repo, context.target.paths);
        fe::runGitCommand(fe::git::Command::Stage, context, *snapshot);
        QTRY_VERIFY_WITH_TIMEOUT(changed, GIT_WAIT_MS);
        QCOMPARE(gitIn(fixture.repo, {QStringLiteral("status"), QStringLiteral("--short")}), QStringLiteral("M  a.txt\n M b.txt"));
    }
};

QTEST_MAIN(GitWidgetTest)
#include "test_gitwidgets.moc"
