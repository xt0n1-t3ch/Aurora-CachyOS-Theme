// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Tests of File Explorer's widgets on Qt's offscreen platform: the path segments' clicks, drops and spring-loading,
// the address field at rest, what assistive technology reads of the path bar, and the filter row over Dolphin's view.

#include "addressfield.h"
#include "bottombar.h"
#include "browsertab.h"
#include "filterbar.h"
#include "tabpage.h"
#include "crumbaccessible.h"
#include "crumbstrip.h"

#include <QAccessible>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QLineEdit>
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

} // namespace

class WidgetTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
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
