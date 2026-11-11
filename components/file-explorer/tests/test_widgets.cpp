// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Tests of File Explorer's widgets on Qt's offscreen platform: the path segments' clicks, drops and spring-loading,
// the address field at rest, what assistive technology reads of the path bar, the filter row over Dolphin's view and the
// Rename sheet.

#include "addressfield.h"
#include "batchrenamesheet.h"
#include "bottombar.h"
#include "browsertab.h"
#include "columnsstrip.h"
#include "commandpalette.h"
#include "filterbar.h"
#include "glassfield.h"
#include "inspectorpanel.h"
#include "searchscopebar.h"
#include "toolbar.h"
#include "tabpage.h"
#include "crumbaccessible.h"
#include "crumbstrip.h"

#include <QAccessible>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QLineEdit>
#include <QListView>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTreeWidget>

#include <KIO/FileUndoManager>
#include <QMimeData>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

Q_DECLARE_METATYPE(fe::CrumbOpen)

namespace {

const QUrl HOME = QUrl::fromLocalFile(QStringLiteral("/home/alex"));
const QUrl GAMES = QUrl::fromLocalFile(QStringLiteral("/home/alex/Games"));

/// A path bar wide enough for its segments, shown so it lays out.
std::unique_ptr<fe::PathBar> pathBar(const QList<fe::Crumb> &crumbs)
{
    auto bar = std::make_unique<fe::PathBar>();
    bar->resize(700, 30);
    bar->setCrumbs(crumbs);
    bar->show();
    return bar;
}

QPoint centerOf(const fe::PathBar &bar, qsizetype slot)
{
    return bar.crumbs()->slotRect(slot).center().toPoint();
}

/// A drag of urls over pos, as Qt hands it to a widget; the event says whether the strip took it.
bool takesDrag(fe::CrumbStrip *strip, QPoint pos, const QList<QUrl> &urls)
{
    QMimeData mime;
    mime.setUrls(urls);
    QDragMoveEvent move(QPointF(pos), Qt::CopyAction | Qt::MoveAction | Qt::LinkAction, &mime, Qt::LeftButton, Qt::NoModifier);
    move.setAccepted(false);
    strip->dragMove(&move);
    return move.isAccepted();
}

/// The Rename button of a Rename sheet.
QPushButton *renameButton(const fe::BatchRenameSheet *sheet)
{
    const auto buttons = sheet->findChildren<QPushButton *>();
    const auto found = std::find_if(buttons.cbegin(), buttons.cend(), [](const QPushButton *b) { return b->text() == QStringLiteral("Rename"); });
    return found == buttons.cend() ? nullptr : *found;
}

} // namespace

class WidgetTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // Settings the widgets write (the archive choice, the search scope) go to a test folder, not the user's.
        QStandardPaths::setTestModeEnabled(true);
        qRegisterMetaType<fe::CrumbOpen>();
        fe::installCrumbAccessibility();
    }

    void aClickOnASegmentOpensItHere()
    {
        const auto bar = pathBar({{QStringLiteral("alex"), HOME}, {QStringLiteral("Games"), GAMES}});
        QSignalSpy opened(bar->crumbs(), &fe::CrumbStrip::crumbActivated);
        QTest::mouseClick(bar.get(), Qt::LeftButton, Qt::NoModifier, centerOf(*bar, 1));
        QCOMPARE(opened.size(), 1);
        QCOMPARE(opened.at(0).at(0).toUrl(), GAMES);
        QCOMPARE(opened.at(0).at(1).value<fe::CrumbOpen>(), fe::CrumbOpen::Here);
    }

    void aMiddleClickOrCtrlClickOpensItInANewTab()
    {
        const auto bar = pathBar({{QStringLiteral("alex"), HOME}, {QStringLiteral("Games"), GAMES}});
        QSignalSpy opened(bar->crumbs(), &fe::CrumbStrip::crumbActivated);
        QTest::mouseClick(bar.get(), Qt::MiddleButton, Qt::NoModifier, centerOf(*bar, 0));
        QTest::mouseClick(bar.get(), Qt::LeftButton, Qt::ControlModifier, centerOf(*bar, 1));
        QCOMPARE(opened.size(), 2);
        QCOMPARE(opened.at(0).at(0).toUrl(), HOME);
        QCOMPARE(opened.at(0).at(1).value<fe::CrumbOpen>(), fe::CrumbOpen::NewTab);
        QCOMPARE(opened.at(1).at(1).value<fe::CrumbOpen>(), fe::CrumbOpen::NewTab);
    }

    void aClickBesideTheSegmentsOpensNothing()
    {
        const auto bar = pathBar({{QStringLiteral("alex"), HOME}});
        QSignalSpy opened(bar->crumbs(), &fe::CrumbStrip::crumbActivated);
        QTest::mouseClick(bar.get(), Qt::LeftButton, Qt::NoModifier, QPoint(500, 15));
        QCOMPARE(opened.size(), 0);
    }

    void aSlotThatReplacesTheSegmentsStillGetsItsUrl()
    {
        // Opening a folder replaces the segments while the signal is still being delivered; the URL must outlive them.
        const auto bar = pathBar({{QStringLiteral("alex"), HOME}, {QStringLiteral("Games"), GAMES}});
        QUrl seen;
        connect(bar->crumbs(), &fe::CrumbStrip::crumbActivated, this, [&](const QUrl &url, fe::CrumbOpen) {
            bar->setCrumbs({{QStringLiteral("other"), QUrl::fromLocalFile(QStringLiteral("/srv/other/place/far/away"))}});
            seen = url;
        });
        QTest::mouseClick(bar.get(), Qt::LeftButton, Qt::NoModifier, centerOf(*bar, 1));
        QCOMPARE(seen, GAMES);
    }

    void aSegmentTakesFilesFromElsewhereAndRefusesItsOwn()
    {
        QTemporaryDir root;
        QVERIFY(root.isValid());
        const QString folder = root.filePath(QStringLiteral("folder"));
        const QString inside = folder + QStringLiteral("/inside");
        QVERIFY(QDir().mkpath(inside));
        QFile file(root.filePath(QStringLiteral("file.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const auto bar = pathBar({{QStringLiteral("folder"), QUrl::fromLocalFile(folder)},
                                  {QStringLiteral("inside"), QUrl::fromLocalFile(inside)}});
        fe::CrumbStrip *strip = bar->crumbs();
        QVERIFY(takesDrag(strip, centerOf(*bar, 0), {QUrl::fromLocalFile(file.fileName())}));
        QVERIFY(!takesDrag(strip, centerOf(*bar, 1), {QUrl::fromLocalFile(folder)}));
        QVERIFY(!takesDrag(strip, centerOf(*bar, 0), {QUrl::fromLocalFile(inside)}));
        QVERIFY(!takesDrag(strip, QPoint(650, 15), {QUrl::fromLocalFile(file.fileName())}));
    }

    void aReadOnlyFolderRefusesFiles()
    {
        QTemporaryDir root;
        const QString locked = root.filePath(QStringLiteral("locked"));
        QVERIFY(QDir().mkpath(locked));
        QVERIFY(QFile::setPermissions(locked, QFile::ReadOwner | QFile::ExeOwner));
        const auto bar = pathBar({{QStringLiteral("locked"), QUrl::fromLocalFile(locked)}, {QStringLiteral("alex"), HOME}});
        QVERIFY(!takesDrag(bar->crumbs(), centerOf(*bar, 0), {QUrl::fromLocalFile(root.filePath(QStringLiteral("x")))}));
        QFile::setPermissions(locked, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    }

    void aDragRestingOnASegmentOpensIt()
    {
        QTemporaryDir root;
        const QString a = root.filePath(QStringLiteral("a"));
        const QString b = a + QStringLiteral("/b");
        QVERIFY(QDir().mkpath(b));
        const auto bar = pathBar({{QStringLiteral("a"), QUrl::fromLocalFile(a)}, {QStringLiteral("b"), QUrl::fromLocalFile(b)}});
        QSignalSpy opened(bar->crumbs(), &fe::CrumbStrip::crumbActivated);
        QVERIFY(takesDrag(bar->crumbs(), centerOf(*bar, 0), {QUrl::fromLocalFile(QStringLiteral("/etc/hostname"))}));
        QVERIFY(opened.wait(2000));
        QCOMPARE(opened.at(0).at(0).toUrl(), QUrl::fromLocalFile(a));
        // The folder on screen is open already.
        QSignalSpy again(bar->crumbs(), &fe::CrumbStrip::crumbActivated);
        QVERIFY(takesDrag(bar->crumbs(), centerOf(*bar, 1), {QUrl::fromLocalFile(QStringLiteral("/etc/hostname"))}));
        QVERIFY(!again.wait(1200));
    }

    void theAddressFieldAtRestOpensASegmentWithoutTakingTheKeyboard()
    {
        fe::AddressField field(QStringLiteral("/home/alex"));
        field.resize(500, 34);
        field.setLocation(GAMES);
        field.setCrumbs({{QStringLiteral("alex"), HOME}, {QStringLiteral("Games"), GAMES}});
        field.show();
        field.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&field));
        field.clearFocus();
        QSignalSpy opened(field.crumbs(), &fe::CrumbStrip::crumbActivated);
        QTest::mouseClick(&field, Qt::LeftButton, Qt::NoModifier, field.crumbs()->slotRect(0).center().toPoint());
        QCOMPARE(opened.size(), 1);
        QVERIFY(!field.hasFocus());
        // Beside the segments the field opens for typing.
        QTest::mouseClick(&field, Qt::LeftButton, Qt::NoModifier, QPoint(460, 17));
        QVERIFY(field.hasFocus());
        QCOMPARE(opened.size(), 1);
    }

    void assistiveTechnologyReadsEachSegmentAsALink()
    {
        const auto bar = pathBar({{QStringLiteral("alex"), HOME}, {QStringLiteral("Games"), GAMES}});
        QAccessibleInterface *group = QAccessible::queryAccessibleInterface(bar.get());
        QVERIFY(group);
        QCOMPARE(group->role(), QAccessible::Grouping);
        QCOMPARE(group->childCount(), 2);
        QAccessibleInterface *games = group->child(1);
        QVERIFY(games);
        QCOMPARE(games->role(), QAccessible::Link);
        QCOMPARE(games->text(QAccessible::Name), QStringLiteral("Games"));
        QCOMPARE(games->text(QAccessible::Description), QStringLiteral("/home/alex/Games"));
        QCOMPARE(group->indexOfChild(games), 1);
        QSignalSpy opened(bar->crumbs(), &fe::CrumbStrip::crumbActivated);
        games->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
        QCOMPARE(opened.size(), 1);
        QCOMPARE(opened.at(0).at(0).toUrl(), GAMES);
        // New segments, new children.
        bar->setCrumbs({{QStringLiteral("alex"), HOME}});
        QCOMPARE(group->childCount(), 1);
        QCOMPARE(group->child(0)->text(QAccessible::Name), QStringLiteral("alex"));
    }

    void theFilterRowSendsEachKeyAndClosesOnEscape()
    {
        QWidget window;
        auto *bar = new fe::FilterBar(&window);
        window.resize(600, 40);
        window.show();
        QVERIFY(bar->isHidden());
        bar->open();
        QVERIFY(bar->isVisible());
        QSignalSpy changed(bar, &fe::FilterBar::filterChanged);
        QSignalSpy toView(bar, &fe::FilterBar::focusViewRequested);
        auto *field = bar->findChild<QLineEdit *>();
        QVERIFY(field);
        QTest::keyClicks(field, QStringLiteral("re"));
        QCOMPARE(changed.size(), 2);
        QCOMPARE(changed.last().at(0).toString(), QStringLiteral("re"));
        bar->setMatchCount(3);
        QCOMPARE(bar->matchText(), QStringLiteral("3 matches"));
        QTest::keyClick(field, Qt::Key_Down);
        QCOMPARE(toView.size(), 1);
        QVERIFY(bar->isVisible());
        QTest::keyClick(field, Qt::Key_Escape);
        QVERIFY(bar->isHidden());
        QCOMPARE(bar->text(), QString());
        QCOMPARE(changed.last().at(0).toString(), QString());
        QCOMPARE(toView.size(), 2);
        // Closing an empty row sends nothing.
        bar->open();
        bar->dismiss();
        QCOMPARE(changed.size(), 3);
    }

    void aFilterHidesWhatDoesNotMatchAndEndsWithTheFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        for (const char *name : {"Report.pdf", "report-draft.odt", "beach.jpg"}) {
            QFile file(dir.filePath(QString::fromLatin1(name)));
            QVERIFY(file.open(QIODevice::WriteOnly));
        }
        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("Other")));
        fe::BrowserTab tab(QUrl::fromLocalFile(dir.path()));
        if (!tab.isLoaded()) {
            QSKIP("Dolphin's part is not installed");
        }
        tab.resize(800, 500);
        tab.show();
        QTRY_COMPARE(tab.statusText().section(u',', 0, 0), QStringLiteral("4 items"));
        tab.openFilter();
        auto *field = tab.filterBar()->findChild<QLineEdit *>();
        QTest::keyClicks(field, QStringLiteral("REPORT"));
        QCOMPARE(tab.statusText().section(u',', 0, 0), QStringLiteral("2 items"));
        QCOMPARE(tab.filterBar()->matchText(), QStringLiteral("2 matches"));
        QTest::keyClicks(field, QStringLiteral("zzz"));
        QCOMPARE(tab.filterBar()->matchText(), QStringLiteral("No matches"));
        // Another folder shows everything it holds, with the row closed.
        tab.openFolder(QUrl::fromLocalFile(dir.filePath(QStringLiteral("Other"))));
        QVERIFY(tab.filterBar()->isHidden());
        QCOMPARE(tab.filterText(), QString());
        tab.goBack();
        QTRY_COMPARE(tab.statusText().section(u',', 0, 0), QStringLiteral("4 items"));
    }

    void theRenameSheetRenamesABatchThatOneUndoTakesBack()
    {
        QTemporaryDir dir;
        const QStringList names{QStringLiteral("IMG_001.jpg"), QStringLiteral("IMG_002.jpg"), QStringLiteral("notes.txt")};
        KFileItemList items;
        for (const QString &name : names) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            items.append(KFileItem(QUrl::fromLocalFile(dir.filePath(name))));
        }
        QWidget window;
        auto *sheet = new fe::BatchRenameSheet(items, &window);
        fe::RenameRule rule;
        rule.find = QStringLiteral("img_");
        rule.replaceWith = QStringLiteral("Trip ");
        sheet->setRule(rule);
        QCOMPARE(sheet->plan().changed, 2);
        auto *preview = sheet->findChild<QTreeWidget *>();
        QCOMPARE(preview->topLevelItem(1)->text(1), QStringLiteral("Trip 002.jpg"));
        QSignalSpy renamed(sheet, &fe::BatchRenameSheet::renamed);
        renameButton(sheet)->click();
        QVERIFY(renamed.wait(5000));
        QCOMPARE(renamed.at(0).at(0).value<QList<QUrl>>().size(), 2);
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("Trip 001.jpg"))));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("Trip 002.jpg"))));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("notes.txt"))));

        KIO::FileUndoManager *undo = KIO::FileUndoManager::self();
        QTRY_VERIFY(undo->isUndoAvailable());
        QSignalSpy undone(undo, &KIO::FileUndoManager::undoJobFinished);
        undo->undo();
        QVERIFY(undone.wait(5000));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("IMG_001.jpg"))));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("IMG_002.jpg"))));
    }

    void theRenameSheetWaitsWhileANameIsTaken()
    {
        QTemporaryDir dir;
        KFileItemList items;
        for (const char *name : {"draft-a.txt", "draft-b.txt"}) {
            QFile file(dir.filePath(QString::fromLatin1(name)));
            QVERIFY(file.open(QIODevice::WriteOnly));
            items.append(KFileItem(QUrl::fromLocalFile(file.fileName())));
        }
        QFile taken(dir.filePath(QStringLiteral("a.txt")));
        QVERIFY(taken.open(QIODevice::WriteOnly));
        QWidget window;
        auto *sheet = new fe::BatchRenameSheet(items, &window);
        fe::RenameRule rule;
        rule.find = QStringLiteral("draft-");
        sheet->setRule(rule);
        QVERIFY(!sheet->plan().canRun());
        QVERIFY(!renameButton(sheet)->isEnabled());
        // A name that leaves the folder while the sheet is open is free when Rename is pressed, which plans again.
        QVERIFY(taken.remove());
        QSignalSpy renamed(sheet, &fe::BatchRenameSheet::renamed);
        QMetaObject::invokeMethod(sheet, "accept");
        QVERIFY(renamed.wait(5000));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("a.txt"))));
        QVERIFY(QFile::exists(dir.filePath(QStringLiteral("b.txt"))));
    }

    void renameWithSeveralItemsSelectedOpensTheSheet()
    {
        QTemporaryDir dir;
        QList<QUrl> urls;
        for (const char *name : {"one.txt", "two.txt"}) {
            QFile file(dir.filePath(QString::fromLatin1(name)));
            QVERIFY(file.open(QIODevice::WriteOnly));
            urls.append(QUrl::fromLocalFile(file.fileName()));
        }
        fe::BrowserTab tab(QUrl::fromLocalFile(dir.path()));
        if (!tab.isLoaded()) {
            QSKIP("Dolphin's part is not installed");
        }
        tab.resize(800, 500);
        tab.show();
        tab.selectInPlace(urls);
        QTRY_COMPARE(tab.selectedItems().size(), 2);
        tab.triggerPartAction(QStringLiteral("renamefile"));
        auto *sheet = tab.window()->findChild<fe::BatchRenameSheet *>();
        QVERIFY(sheet);
        QCOMPARE(sheet->windowTitle(), QStringLiteral("Rename 2 Items"));
        sheet->close();
    }

    void copyPathTakesEverySelectedItemOrTheFolder()
    {
        QTemporaryDir dir;
        QList<QUrl> urls;
        for (const char *name : {"one.txt", "two.txt", "three"}) {
            const QString path = dir.filePath(QString::fromLatin1(name));
            if (QByteArrayView(name) == "three") {
                QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("three")));
            } else {
                QFile file(path);
                QVERIFY(file.open(QIODevice::WriteOnly));
            }
            urls.append(QUrl::fromLocalFile(path));
        }
        fe::BrowserTab tab(QUrl::fromLocalFile(dir.path()));
        if (!tab.isLoaded()) {
            QSKIP("Dolphin's part is not installed");
        }
        tab.resize(800, 500);
        tab.show();
        QTRY_COMPARE(tab.statusText().section(u',', 0, 0), QStringLiteral("3 items"));
        // Nothing selected: the folder.
        QApplication::clipboard()->clear();
        tab.partAction(QStringLiteral("copy_location"))->trigger();
        QCOMPARE(QApplication::clipboard()->text(), dir.path());
        tab.selectInPlace(urls);
        QTRY_COMPARE(tab.selectedItems().size(), 3);
        tab.partAction(QStringLiteral("copy_location"))->trigger();
        QStringList lines = QApplication::clipboard()->text().split(QLatin1Char('\n'));
        lines.sort();
        QStringList expected{dir.filePath(QStringLiteral("one.txt")), dir.filePath(QStringLiteral("three")), dir.filePath(QStringLiteral("two.txt"))};
        expected.sort();
        QCOMPARE(lines, expected);
    }

    void anArchiveOpensAsAFolderAndUpLeavesIt()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("src/inner")));
        QFile inner(dir.filePath(QStringLiteral("src/inner/readme.txt")));
        QVERIFY(inner.open(QIODevice::WriteOnly));
        inner.write("hi");
        inner.close();
        QVERIFY(QProcess::execute(QStringLiteral("tar"), {QStringLiteral("-czf"), dir.filePath(QStringLiteral("pack.tar.gz")), QStringLiteral("-C"),
                                                         dir.filePath(QStringLiteral("src")), QStringLiteral("inner")}) == 0);
        QFile document(dir.filePath(QStringLiteral("letter.odt")));
        QVERIFY(document.open(QIODevice::WriteOnly));
        document.write("PK");
        document.close();
        const QUrl archive = QUrl::fromLocalFile(dir.filePath(QStringLiteral("pack.tar.gz")));
        fe::BrowserTab::setOpensArchivesAsFolders(true);
        QCOMPARE(fe::BrowserTab::archiveFolderUrl(archive, {}).scheme(), QStringLiteral("tar"));
        QCOMPARE(fe::BrowserTab::archiveFolderUrl(QUrl::fromLocalFile(document.fileName()), {}), QUrl());
        QCOMPARE(fe::BrowserTab::archiveFolderUrl(QUrl::fromLocalFile(dir.path()), {}), QUrl());

        fe::BrowserTab tab(QUrl::fromLocalFile(dir.path()));
        if (!tab.isLoaded()) {
            QSKIP("Dolphin's part is not installed");
        }
        tab.resize(800, 500);
        tab.show();
        KFileItem item(archive);
        item.refresh();
        tab.activateItems({item});
        QCOMPARE(tab.url().scheme(), QStringLiteral("tar"));
        QTRY_COMPARE(tab.statusText().section(u',', 0, 0), QStringLiteral("1 item"));
        tab.goUp();
        QCOMPARE(tab.url().adjusted(QUrl::StripTrailingSlash), QUrl::fromLocalFile(dir.path()));

        // The Inspector reads the archive's folder through KIO.
        fe::InspectorPanel panel;
        panel.resize(290, 700);
        panel.show();
        QSignalSpy counted(&panel, &fe::InspectorPanel::sizeCounted);
        panel.setItems({}, fe::BrowserTab::archiveFolderUrl(archive, {}));
        QVERIFY(counted.wait(5000));
        QVERIFY2(panel.sizeText().endsWith(QStringLiteral(", 2 items")), qPrintable(panel.sizeText()));

        // Turned off, the archive opens in its application instead: the tab stays where it is.
        fe::BrowserTab::setOpensArchivesAsFolders(false);
        QCOMPARE(fe::BrowserTab::archiveFolderUrl(archive, {}), QUrl());
        fe::BrowserTab::setOpensArchivesAsFolders(true);
    }

    void theInspectorCountsAFolderAndCountsAgainWhenItChanges()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("sub")));
        const auto write = [&dir](const QString &name, qsizetype bytes) {
            QFile file(dir.filePath(name));
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QByteArray(bytes, 'x'));
        };
        write(QStringLiteral("a.bin"), 2000);
        write(QStringLiteral("sub/b.bin"), 3000);
        fe::InspectorPanel panel;
        panel.resize(290, 700);
        QSignalSpy counted(&panel, &fe::InspectorPanel::sizeCounted);
        panel.setItems({}, QUrl::fromLocalFile(dir.path()));
        // Hidden, it works out nothing.
        QVERIFY(!counted.wait(300));
        panel.show();
        QVERIFY(counted.wait(5000));
        QVERIFY2(panel.sizeText().endsWith(QStringLiteral("3 items")), qPrintable(panel.sizeText()));
        // A new item in the folder counts it again.
        write(QStringLiteral("c.bin"), 10);
        QVERIFY(counted.wait(5000));
        QVERIFY2(panel.sizeText().endsWith(QStringLiteral("4 items")), qPrintable(panel.sizeText()));
    }

    void theInspectorHashesAFileAndComparesAPastedHash()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath(QStringLiteral("abc.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("abc");
        file.close();
        fe::InspectorPanel panel;
        panel.resize(290, 700);
        panel.show();
        KFileItem item(QUrl::fromLocalFile(file.fileName()));
        item.refresh();
        panel.setItems({item}, QUrl::fromLocalFile(dir.path()));
        auto *compute = panel.findChildren<fe::GlassButton *>().value(0);
        QVERIFY(compute);
        QSignalSpy hashed(&panel, &fe::InspectorPanel::hashComputed);
        compute->click();
        QVERIFY(hashed.wait(5000));
        QCOMPARE(panel.hashText(), QStringLiteral("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
        auto *compare = panel.findChild<fe::GlassLineEdit *>();
        compare->setText(QStringLiteral("BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD  abc.txt"));
        QVERIFY(panel.compareText().startsWith(QStringLiteral("Matches")));
        compare->setText(QStringLiteral("0000000000000000000000000000000000000000000000000000000000000000"));
        QVERIFY(panel.compareText().startsWith(QStringLiteral("Does not match")));
    }

    void theColumnsLeadToTheFolderAndOpenWhatIsClicked()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkpath(QStringLiteral("a/b")));
        QFile file(dir.filePath(QStringLiteral("a/note.txt")));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();
        const QUrl a = QUrl::fromLocalFile(dir.filePath(QStringLiteral("a")));
        const QUrl b = QUrl::fromLocalFile(dir.filePath(QStringLiteral("a/b")));
        fe::BrowserTab tab(b);
        if (!tab.isLoaded()) {
            QSKIP("Dolphin's part is not installed");
        }
        tab.resize(1200, 500);
        tab.show();
        fe::ColumnsStrip *strip = tab.columnsStrip();
        // The mode the last tab was left in comes back; this test starts from the icons.
        tab.setViewMode(fe::ViewMode::Icons);
        QVERIFY(strip->isHidden());
        tab.setViewMode(fe::ViewMode::Columns);
        QCOMPARE(tab.viewMode(), fe::ViewMode::Columns);
        QVERIFY(strip->isVisible());
        QCOMPARE(strip->folders().constLast(), a);
        // The last column lists a: b, marked as the folder on screen, and note.txt.
        QListView *last = strip->column(strip->folders().size() - 1);
        QTRY_COMPARE(last->model()->rowCount(), 2);
        QTRY_COMPARE(last->currentIndex().data().toString(), QStringLiteral("b"));
        // A click on the file opens its folder with it selected; a click on a folder opens it.
        const QModelIndex note = last->model()->index(1, 0);
        QCOMPARE(note.data().toString(), QStringLiteral("note.txt"));
        QTest::mouseClick(last->viewport(), Qt::LeftButton, Qt::NoModifier, last->visualRect(note).center());
        QTRY_COMPARE(tab.url().adjusted(QUrl::StripTrailingSlash), a);
        QCOMPARE(strip->folders().constLast(), QUrl::fromLocalFile(QDir::cleanPath(dir.path())));
        // Another mode takes the columns away.
        tab.setViewMode(fe::ViewMode::Icons);
        QVERIFY(strip->isHidden());
        QCOMPARE(tab.viewMode(), fe::ViewMode::Icons);
    }

    void theSearchRowSavesAndHidesTheFolderTheIndexDoesNotHold()
    {
        fe::SearchScopeBar bar;
        bar.resize(700, 34);
        bar.setScope(QStringLiteral("Games"), true);
        bar.show();
        QSignalSpy saved(&bar, &fe::SearchScopeBar::saveRequested);
        QSignalSpy chosen(&bar, &fe::SearchScopeBar::scopeChosen);
        // Save sits at the right end.
        QTest::mouseClick(&bar, Qt::LeftButton, Qt::NoModifier, QPoint(660, 17));
        QCOMPARE(saved.size(), 1);
        // The folder's capsule, right of This PC, picks the folder; hidden, it is not there to press.
        const QPoint folder(170, 17);
        QTest::mouseClick(&bar, Qt::LeftButton, Qt::NoModifier, folder);
        QCOMPARE(chosen.size(), 1);
        bar.setScope(QStringLiteral("Games"), true);
        bar.setFolderShown(false);
        QTest::mouseClick(&bar, Qt::LeftButton, Qt::NoModifier, folder);
        QCOMPARE(chosen.size(), 1);
    }

    void theCommandPaletteFindsAndRunsACommand()
    {
        QWidget window;
        window.resize(900, 600);
        QAction newFolder(QStringLiteral("New Folder"));
        QAction newTab(QStringLiteral("New Tab"));
        QAction hidden(QStringLiteral("Show Hidden Files"));
        QAction off(QStringLiteral("Paste"));
        off.setEnabled(false);
        QSignalSpy made(&newFolder, &QAction::triggered);
        QSignalSpy pasted(&off, &QAction::triggered);
        auto *palette = new fe::CommandPalette(&window);
        window.show();
        const QList<fe::CommandPalette::Command> commands{{&newTab, QStringLiteral("File")}, {&newFolder, QStringLiteral("File")},
                                                          {&hidden, QStringLiteral("View")}, {&off, QStringLiteral("Edit")}};
        palette->open(commands);
        QVERIFY(palette->isVisible());
        QCOMPARE(palette->shownTexts().size(), 4);
        auto *field = palette->findChild<QLineEdit *>();
        QTest::keyClicks(field, QStringLiteral("nf"));
        QCOMPARE(palette->chosenText(), QStringLiteral("New Folder"));
        QTest::keyClick(field, Qt::Key_Return);
        QCOMPARE(made.size(), 1);
        QVERIFY(palette->isHidden());

        // Down moves the choice; a disabled command does not run; Escape closes without running anything.
        palette->open(commands);
        QTest::keyClicks(field, QStringLiteral("new"));
        QCOMPARE(palette->chosenText(), QStringLiteral("New Tab"));
        QTest::keyClick(field, Qt::Key_Down);
        QCOMPARE(palette->chosenText(), QStringLiteral("New Folder"));
        QTest::keyClick(field, Qt::Key_Escape);
        QVERIFY(palette->isHidden());
        QCOMPARE(made.size(), 1);
        palette->open(commands);
        QTest::keyClicks(field, QStringLiteral("paste"));
        QTest::keyClick(field, Qt::Key_Return);
        QCOMPARE(pasted.size(), 0);
    }

    void aFieldInsideATabShowsTheGlassBehindIt()
    {
        // A style sheet on any ancestor hands every field below it to Qt's style sheet style, which fills a line
        // edit with an opaque panel; the window's glass must show through the filter field instead.
        QTemporaryDir dir;
        auto *tab = new fe::BrowserTab(QUrl::fromLocalFile(dir.path()));
        if (!tab->isLoaded()) {
            delete tab;
            QSKIP("Dolphin's part is not installed");
        }
        fe::TabPage page(tab);
        page.resize(800, 400);
        page.show();
        tab->openFilter();
        auto *field = tab->filterBar()->findChild<QLineEdit *>();
        QImage image(page.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        page.render(&image, QPoint(), QRegion(), QWidget::DrawChildren);
        const QColor inside = image.pixelColor(field->mapTo(&page, QPoint(field->width() / 2, field->height() / 2)));
        QVERIFY2(inside.alphaF() < 0.5, qPrintable(inside.name(QColor::HexArgb)));
    }
};

QTEST_MAIN(WidgetTest)
#include "test_widgets.moc"
