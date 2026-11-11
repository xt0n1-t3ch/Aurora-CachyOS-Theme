// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Unit tests for the widget-free logic in src/core: panels, FileManager1 grouping, history, paths, edges, shortcut
// labels, status text, sidebar rules, drives and the command palette's ranking.

#include "core/edges.h"
#include "core/format.h"
#include "core/history.h"
#include "core/paths.h"
#include "core/places.h"
#include "core/chooser.h"
#include "core/drives.h"
#include "core/reveal.h"
#include "core/motion.h"
#include "core/palette.h"

#include <algorithm>

#include <QKeySequence>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSet>
#include <QTest>
#include <QUrlQuery>


namespace {

// A local folder URL for path.
QUrl folder(const QString &path)
{
    return QUrl::fromLocalFile(path);
}

// A size formatter that shows the byte count exactly, so expected texts do not depend on locale or units.
QString bytes(qint64 count)
{
    return QStringLiteral("[%1 B]").arg(count);
}

// A user "alex" with home /home/alex, whose system disk is named "CachyOS".
const fe::PathContext HOME_CONTEXT{QStringLiteral("/home/alex"), QStringLiteral("alex"), QStringLiteral("CachyOS")};

/// A context menu as the reader sees it: each entry's text, a dash line between groups, dimmed entries marked.
QStringList rendered(const QList<QList<fe::SidebarMenuEntry>> &menu, const QString &title = QStringLiteral("USB"))
{
    QStringList lines;
    for (const QList<fe::SidebarMenuEntry> &group : menu) {
        if (!lines.isEmpty()) {
            lines.append(QStringLiteral("—"));
        }
        for (const fe::SidebarMenuEntry &entry : group) {
            lines.append(fe::sidebarCommandText(entry.command, title) + (entry.enabled ? QString() : QStringLiteral(" (dimmed)")));
        }
    }
    return lines;
}

/// The commands of a menu in order, without the group boundaries.
QList<fe::SidebarCommand> commandsIn(const QList<QList<fe::SidebarMenuEntry>> &menu)
{
    QList<fe::SidebarCommand> commands;
    for (const QList<fe::SidebarMenuEntry> &group : menu) {
        for (const fe::SidebarMenuEntry &entry : group) {
            commands.append(entry.command);
        }
    }
    return commands;
}

// A sidebar that shows no folder yet.
const auto NOTHING_IN_SIDEBAR = [](const QUrl &) { return false; };

/// A plain two-stack model of browser history, written independently of NavigationHistory.
struct ReferenceHistory {
    QList<QUrl> backStack;
    QList<QUrl> forwardStack;
    QUrl current;

    void visit(const QUrl &url)
    {
        if (!current.isEmpty() && current.matches(url, QUrl::StripTrailingSlash)) {
            return;
        }
        if (!current.isEmpty()) {
            backStack.append(current);
        }
        forwardStack.clear();
        current = url;
    }
    void back()
    {
        if (!backStack.isEmpty()) {
            forwardStack.append(current);
            current = backStack.takeLast();
        }
    }
    void forward()
    {
        if (!forwardStack.isEmpty()) {
            backStack.append(current);
            current = forwardStack.takeLast();
        }
    }
};

} // namespace

/// Every test of src/core in one QtTest object; each slot is named after the behavior it pins down.
class CoreTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // Open and Save panels for the FileChooser portal (org.freedesktop.impl.portal.FileChooser).
    void aFilterShowsFilesByGlobOrMimeType()
    {
        const fe::NameFilter images{QStringLiteral("Images"), {QStringLiteral("*.png"), QStringLiteral("*.JPG")}, {QStringLiteral("image/webp")}};
        QVERIFY(fe::filterAccepts(images, QStringLiteral("cat.png"), QStringLiteral("image/png")));
        QVERIFY(fe::filterAccepts(images, QStringLiteral("dog.jpg"), QStringLiteral("image/jpeg")));
        QVERIFY(fe::filterAccepts(images, QStringLiteral("bird"), QStringLiteral("image/webp")));
        QVERIFY(!fe::filterAccepts(images, QStringLiteral("notes.txt"), QStringLiteral("text/plain")));
        QVERIFY(fe::filterAccepts(fe::NameFilter{}, QStringLiteral("anything.bin"), QStringLiteral("application/octet-stream")));
    }

    void theViewPatternJoinsGlobsAndMimeGlobsOnce()
    {
        const fe::NameFilter images{QStringLiteral("Images"), {QStringLiteral("*.png")}, {QStringLiteral("image/jpeg")}};
        QCOMPARE(fe::viewPattern(images, {QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.png")}),
                 QStringLiteral("*.png *.jpg *.jpeg"));
    }

    void savingUnderAOneTypeFilterAddsItsExtension()
    {
        const fe::NameFilter png{QStringLiteral("PNG"), {QStringLiteral("*.png")}, {}};
        QCOMPARE(fe::nameForSave(QStringLiteral("  Screenshot "), png), QStringLiteral("Screenshot.png"));
        QCOMPARE(fe::nameForSave(QStringLiteral("photo.jpeg"), png), QStringLiteral("photo.jpeg"));
        const fe::NameFilter two{QStringLiteral("Images"), {QStringLiteral("*.png"), QStringLiteral("*.jpg")}, {}};
        QCOMPARE(fe::nameForSave(QStringLiteral("Screenshot"), two), QStringLiteral("Screenshot"));
    }

    void theSaveTargetIsTheNameInTheFolder()
    {
        QCOMPARE(fe::saveTarget(folder(QStringLiteral("/home/alex/Documents/")), QStringLiteral("a b.txt")),
                 folder(QStringLiteral("/home/alex/Documents/a b.txt")));
    }

    void anOpenPanelAnswersOneFileUnlessSeveralAreAllowed()
    {
        const QList<QUrl> files{folder(QStringLiteral("/a/1")), folder(QStringLiteral("/a/2"))};
        const QList<QUrl> folders{folder(QStringLiteral("/a/sub"))};
        QCOMPARE(fe::openResult(files, folders, folder(QStringLiteral("/a")), false, false), QList<QUrl>{files.first()});
        QCOMPARE(fe::openResult(files, folders, folder(QStringLiteral("/a")), false, true), files);
        QCOMPARE(fe::openResult(files, folders, folder(QStringLiteral("/a")), true, false), folders);
        QCOMPARE(fe::openResult(files, {}, folder(QStringLiteral("/a")), true, false), QList<QUrl>{folder(QStringLiteral("/a"))});
    }

    void portalPathsDropTheirTrailingNul()
    {
        QCOMPARE(fe::portalPath(QByteArray("/home/alex\0", 11)), QStringLiteral("/home/alex"));
        QCOMPARE(fe::portalPath(QByteArray("/tmp")), QStringLiteral("/tmp"));
    }

    // org.freedesktop.FileManager1, as browsers and Steam call it for "Show in folder".
    void showItemsOpensOneWindowPerFolderSelectingItsItems()
    {
        const QList<fe::Reveal> groups = fe::revealGroups({QStringLiteral("file:///home/alex/Downloads/a.zip"),
                                                           QStringLiteral("file:///home/alex/Music/b.flac"),
                                                           QStringLiteral("file:///home/alex/Downloads/c.png")});
        QCOMPARE(groups.size(), 2);
        QCOMPARE(groups[0].folder, folder(QStringLiteral("/home/alex/Downloads")));
        QCOMPARE(groups[0].items, (QList<QUrl>{folder(QStringLiteral("/home/alex/Downloads/a.zip")),
                                               folder(QStringLiteral("/home/alex/Downloads/c.png"))}));
        QCOMPARE(groups[1].folder, folder(QStringLiteral("/home/alex/Music")));
        QCOMPARE(groups[1].items, QList<QUrl>{folder(QStringLiteral("/home/alex/Music/b.flac"))});
    }

    void showItemsOnAFolderSelectsItInItsParent()
    {
        const QList<fe::Reveal> groups = fe::revealGroups({QStringLiteral("file:///home/alex/Games/")});
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].folder, folder(QStringLiteral("/home/alex")));
        QCOMPARE(groups[0].items, QList<QUrl>{folder(QStringLiteral("/home/alex/Games"))});
    }

    void showItemsAcceptsPlainPaths()
    {
        const QList<fe::Reveal> groups = fe::revealGroups({QStringLiteral("/home/alex/notes.txt")});
        QCOMPARE(groups.size(), 1);
        QCOMPARE(groups[0].folder, folder(QStringLiteral("/home/alex")));
    }

    void showFoldersOpensEachFolderOnce()
    {
        QCOMPARE(fe::foldersToShow({QStringLiteral("file:///home/alex/Downloads"), QStringLiteral("/home/alex/Downloads/"),
                                    QStringLiteral("file:///mnt/D")}),
                 (QList<QUrl>{folder(QStringLiteral("/home/alex/Downloads")), folder(QStringLiteral("/mnt/D"))}));
    }

    void goingBackThenForwardReturnsToTheSameFolder()
    {
        NavigationHistory history;
        history.visit(folder(QStringLiteral("/home/alex")));
        history.visit(folder(QStringLiteral("/home/alex/Games")));

        QCOMPARE(history.back(), folder(QStringLiteral("/home/alex")));
        QCOMPARE(history.forward(), folder(QStringLiteral("/home/alex/Games")));
    }

    void aVisitAfterGoingBackDropsTheForwardHistory()
    {
        NavigationHistory history;
        history.visit(folder(QStringLiteral("/home/alex")));
        history.visit(folder(QStringLiteral("/home/alex/Games")));
        history.back();
        history.visit(folder(QStringLiteral("/home/alex/Music")));

        QVERIFY(!history.canGoForward());
        QCOMPARE(history.back(), folder(QStringLiteral("/home/alex")));
    }

    void revisitingTheCurrentFolderAddsNoEntry()
    {
        NavigationHistory history;
        history.visit(folder(QStringLiteral("/home/alex")));
        history.visit(QUrl(QStringLiteral("file:///home/alex/")));

        QVERIFY(!history.canGoBack());
    }

    void backAtTheFirstFolderStaysThere()
    {
        NavigationHistory history;
        history.visit(folder(QStringLiteral("/home/alex")));

        QVERIFY(!history.canGoBack());
        QCOMPARE(history.back(), folder(QStringLiteral("/home/alex")));
    }

    void historyMatchesAReferenceModelForRandomCommandSequences()
    {
        const quint32 seed = QRandomGenerator::global()->generate();
        QRandomGenerator random(seed);
        const QList<QUrl> folders{folder(QStringLiteral("/a")), folder(QStringLiteral("/b")), folder(QStringLiteral("/c")),
                                  folder(QStringLiteral("/d"))};
        for (int run = 0; run < 200; ++run) {
            NavigationHistory history;
            ReferenceHistory reference;
            for (int step = 0; step < 40; ++step) {
                switch (random.bounded(3)) {
                case 0: {
                    const QUrl next = folders.at(random.bounded(folders.size()));
                    history.visit(next);
                    reference.visit(next);
                    break;
                }
                case 1:
                    history.back();
                    reference.back();
                    break;
                default:
                    history.forward();
                    reference.forward();
                    break;
                }
                const QByteArray where = "seed " + QByteArray::number(seed) + ", run " + QByteArray::number(run) + ", step "
                    + QByteArray::number(step);
                QVERIFY2(history.current() == reference.current, where.constData());
                QVERIFY2(history.canGoBack() == !reference.backStack.isEmpty(), where.constData());
                QVERIFY2(history.canGoForward() == !reference.forwardStack.isEmpty(), where.constData());
            }
        }
    }

    void aFolderInsideHomeStartsWithTheUserName()
    {
        const QList<fe::Crumb> crumbs = fe::crumbsFor(folder(QStringLiteral("/home/alex/Games/Mods")), HOME_CONTEXT);

        QCOMPARE(crumbs.size(), 3);
        QCOMPARE(crumbs.at(0).label, QStringLiteral("alex"));
        QCOMPARE(crumbs.at(0).url, folder(QStringLiteral("/home/alex")));
        QCOMPARE(crumbs.at(1).label, QStringLiteral("Games"));
        QCOMPARE(crumbs.at(2).label, QStringLiteral("Mods"));
        QCOMPARE(crumbs.at(2).url, folder(QStringLiteral("/home/alex/Games/Mods")));
    }

    void aFolderOutsideHomeStartsWithTheRootDisk()
    {
        const QList<fe::Crumb> crumbs = fe::crumbsFor(folder(QStringLiteral("/mnt/D")), HOME_CONTEXT);

        QCOMPARE(crumbs.size(), 3);
        QCOMPARE(crumbs.at(0).label, QStringLiteral("CachyOS"));
        QCOMPARE(crumbs.at(0).url, folder(QStringLiteral("/")));
        QCOMPARE(crumbs.at(1).url, folder(QStringLiteral("/mnt")));
        QCOMPARE(crumbs.at(2).label, QStringLiteral("D"));
    }

    void aFolderNamedLikeHomeButOutsideItIsNotHome()
    {
        const QList<fe::Crumb> crumbs = fe::crumbsFor(folder(QStringLiteral("/home/alexfiles")), HOME_CONTEXT);

        QCOMPARE(crumbs.at(0).label, QStringLiteral("CachyOS"));
    }

    void theTrashIsOneCrumbNamedTrash()
    {
        const QList<fe::Crumb> crumbs = fe::crumbsFor(QUrl(QStringLiteral("trash:/")), HOME_CONTEXT);

        QCOMPARE(crumbs.size(), 1);
        QCOMPARE(crumbs.at(0).label, QStringLiteral("Trash"));
        QCOMPARE(fe::folderTitle(QUrl(QStringLiteral("trash:/")), HOME_CONTEXT), QStringLiteral("Trash"));
    }

    void theTitleOfAFolderIsItsLastCrumb()
    {
        QCOMPARE(fe::folderTitle(folder(QStringLiteral("/home/alex/Games")), HOME_CONTEXT), QStringLiteral("Games"));
        QCOMPARE(fe::folderTitle(folder(QStringLiteral("/home/alex")), HOME_CONTEXT), QStringLiteral("alex"));
    }

    void aSearchUrlCarriesTermAndFolderTheWayDolphinReadsThem()
    {
        // Dolphin's DolphinQuery reads these items with QUrl::FullyDecoded
        // (src/search/dolphinquery.cpp, v26.08.2, lines 130-144).
        const QUrl url = fe::filenameSearchUrl(QStringLiteral("save 01 & mods"), folder(QStringLiteral("/home/alex/Games")),
                                               QStringLiteral("Search in Games"));
        const QUrlQuery query(url);

        QCOMPARE(url.scheme(), QStringLiteral("filenamesearch"));
        QCOMPARE(query.queryItemValue(QStringLiteral("search"), QUrl::FullyDecoded), QStringLiteral("save 01 & mods"));
        QCOMPARE(query.queryItemValue(QStringLiteral("url"), QUrl::FullyDecoded), QStringLiteral("file:///home/alex/Games"));
        QCOMPARE(fe::folderTitle(url, HOME_CONTEXT), QStringLiteral("Search in Games"));
    }

    void aPressInTheBottomRightCornerResizesBothEdges()
    {
        QCOMPARE(fe::resizeEdgesAt(QPoint(797, 597), QSize(800, 600), 6), Qt::Edges(Qt::RightEdge | Qt::BottomEdge));
    }

    void aPressOnTheLeftBandResizesOnlyTheLeftEdge()
    {
        QCOMPARE(fe::resizeEdgesAt(QPoint(2, 300), QSize(800, 600), 6), Qt::Edges(Qt::LeftEdge));
    }

    void theResizeBandIsExactlyTheBorderWideOnEverySide()
    {
        // An 800 x 600 window with a 6 px band: columns 794-799 and rows 594-599 resize.
        QCOMPARE(fe::resizeEdgesAt(QPoint(794, 300), QSize(800, 600), 6), Qt::Edges(Qt::RightEdge));
        QCOMPARE(fe::resizeEdgesAt(QPoint(793, 300), QSize(800, 600), 6), Qt::Edges());
        QCOMPARE(fe::resizeEdgesAt(QPoint(400, 594), QSize(800, 600), 6), Qt::Edges(Qt::BottomEdge));
        QCOMPARE(fe::resizeEdgesAt(QPoint(400, 593), QSize(800, 600), 6), Qt::Edges());
        QCOMPARE(fe::resizeEdgesAt(QPoint(400, 5), QSize(800, 600), 6), Qt::Edges(Qt::TopEdge));
    }

    void aPressInsideTheWindowResizesNothing()
    {
        QCOMPARE(fe::resizeEdgesAt(QPoint(6, 300), QSize(800, 600), 6), Qt::Edges());
        QCOMPARE(fe::resizeEdgesAt(QPoint(400, 300), QSize(800, 600), 6), Qt::Edges());
    }

    void aFolderWithNothingSelectedCountsItsItems()
    {
        QCOMPARE(fe::statusLine({5, 0, 0, std::nullopt}, bytes), QStringLiteral("5 items"));
        QCOMPARE(fe::statusLine({1, 0, 0, std::nullopt}, bytes), QStringLiteral("1 item"));
        QCOMPARE(fe::statusLine({0, 0, 0, std::nullopt}, bytes), QStringLiteral("0 items"));
        QCOMPARE(fe::statusLine({5, 0, 0, 120}, bytes), QStringLiteral("5 items, [120 B] available"));
    }

    void shortcutsAreWrittenTheWayWindowsWritesThem()
    {
        // Windows orders modifiers Windows key, Ctrl, Alt, Shift ("Ctrl+Alt+Del", "Alt+Shift+P",
        // "Windows key+E" in Microsoft's "Keyboard shortcuts in Windows"), whatever order Qt wrote them in.
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Ctrl+C")), QStringLiteral("Ctrl+C"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Ctrl+Shift+N")), QStringLiteral("Ctrl+Shift+N"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Shift+Ctrl+N")), QStringLiteral("Ctrl+Shift+N"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Ctrl+Alt+C")), QStringLiteral("Ctrl+Alt+C"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Meta+E")), QStringLiteral("Win+E"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Meta+Ctrl+T")), QStringLiteral("Win+Ctrl+T"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Shift+Alt+P")), QStringLiteral("Alt+Shift+P"));
    }

    void namedKeysUseTheirWindowsNames()
    {
        // Microsoft's spellings: "Alt+Enter", "Alt+Left arrow", "Alt+Page Down" (sentence case in the style
        // guide), "Ctrl+Insert", "Alt+Spacebar"; Win32 menus write the Delete key as "Del".
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Alt+Return")), QStringLiteral("Alt+Enter"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Enter")), QStringLiteral("Enter"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Del")), QStringLiteral("Del"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Shift+Del")), QStringLiteral("Shift+Del"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Alt+Left")), QStringLiteral("Alt+Left arrow"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Shift+Alt+Up")), QStringLiteral("Alt+Shift+Up arrow"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Ctrl+PgDown")), QStringLiteral("Ctrl+Page down"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Ctrl+Ins")), QStringLiteral("Ctrl+Insert"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Alt+Space")), QStringLiteral("Alt+Spacebar"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Backspace")), QStringLiteral("Backspace"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Esc")), QStringLiteral("Esc"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("F2")), QStringLiteral("F2"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Ctrl+[")), QStringLiteral("Ctrl+["));
    }

    void punctuationKeysAreSpelledOut()
    {
        // The style guide spells out Plus, Minus, Comma and Period in shortcuts ("Ctrl+Plus (+)").
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Ctrl++")), QStringLiteral("Ctrl+Plus"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Ctrl+-")), QStringLiteral("Ctrl+Minus"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Ctrl+,")), QStringLiteral("Ctrl+Comma"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("Ctrl+.")), QStringLiteral("Ctrl+Period"));
        QCOMPARE(fe::shortcutLabel(QStringLiteral("+")), QStringLiteral("Plus"));
    }

    void menuShortcutsShowNoMacSymbols()
    {
        // Every key the app binds, as Qt writes it in a menu: the label never falls back to a Mac glyph.
        const QList<QKeyCombination> bound{Qt::CTRL | Qt::Key_X, Qt::CTRL | Qt::ALT | Qt::Key_C, Qt::CTRL | Qt::Key_I,
                                           Qt::CTRL | Qt::META | Qt::Key_T, Qt::CTRL | Qt::SHIFT | Qt::Key_W,
                                           Qt::CTRL | Qt::Key_Down, Qt::ALT | Qt::Key_Return, Qt::SHIFT | Qt::Key_Delete};
        for (const QKeyCombination combination : bound) {
            const QString label = fe::shortcutLabel(QKeySequence(combination).toString(QKeySequence::NativeText));
            QVERIFY2(!label.contains(QRegularExpression(QStringLiteral("[⌃⌥⇧⌘↩⌦⌫←→↑↓]"))), qPrintable(label));
        }
    }

    void aSelectionShowsItsShareAndTheSizeOfItsFiles()
    {
        QCOMPARE(fe::statusLine({32, 1, 41, std::nullopt}, bytes), QStringLiteral("1 of 32 selected, [41 B]"));
        QCOMPARE(fe::statusLine({32, 3, 41, 7}, bytes), QStringLiteral("3 of 32 selected, [41 B], [7 B] available"));
        // Selected folders carry no size, so only the share and the free space remain.
        QCOMPARE(fe::statusLine({5, 2, 0, 120}, bytes), QStringLiteral("2 of 5 selected, [120 B] available"));
    }

    void aStandardFolderOpensAndShowsItsInfoButStaysInTheSidebar()
    {
        const fe::SidebarPlace downloads{.url = folder(QStringLiteral("/home/alex/Downloads"))};
        QCOMPARE(rendered(fe::sidebarMenu(downloads)),
                 (QStringList{QStringLiteral("Open"), QStringLiteral("Open in New Tab"), QStringLiteral("—"), QStringLiteral("Get Info")}));
    }

    void aFolderTheUserAddedCanBeRemovedFromTheSidebar()
    {
        const fe::SidebarPlace projects{.url = folder(QStringLiteral("/home/alex/Projects")), .addedByUser = true};
        QCOMPARE(rendered(fe::sidebarMenu(projects)),
                 (QStringList{QStringLiteral("Open"), QStringLiteral("Open in New Tab"), QStringLiteral("—"), QStringLiteral("Get Info"),
                              QStringLiteral("—"), QStringLiteral("Remove from Sidebar")}));
    }

    void aMountedRemovableDriveIsEjected()
    {
        const fe::SidebarPlace usb{.url = folder(QStringLiteral("/run/media/alex/USB")), .teardownAllowed = true};
        QCOMPARE(rendered(fe::sidebarMenu(usb)),
                 (QStringList{QStringLiteral("Open"), QStringLiteral("Open in New Tab"), QStringLiteral("—"), QStringLiteral("Get Info"),
                              QStringLiteral("—"), QStringLiteral("Eject “USB”")}));
    }

    void aMountedBuiltInDriveIsUnmountedNotEjected()
    {
        const fe::SidebarPlace data{.url = folder(QStringLiteral("/mnt/D")), .teardownAllowed = true, .fixedDrive = true};
        QCOMPARE(rendered(fe::sidebarMenu(data), QStringLiteral("D")).constLast(), QStringLiteral("Unmount “D”"));
    }

    void theSystemDiskIsNeverOfferedForUnmounting()
    {
        // KDE may report the root file system as one more mounted drive; Finder never offers to eject Macintosh HD.
        const fe::SidebarPlace system{.url = folder(QStringLiteral("/")), .teardownAllowed = true, .fixedDrive = true};
        QCOMPARE(rendered(fe::sidebarMenu(system)),
                 (QStringList{QStringLiteral("Open"), QStringLiteral("Open in New Tab"), QStringLiteral("—"), QStringLiteral("Get Info")}));
    }

    void aDriveIsRenamedFromItsMenuAfterGetInfo()
    {
        // Finder: Get Info, then Rename "Macintosh HD".
        const fe::SidebarPlace system{.url = folder(QStringLiteral("/")), .teardownAllowed = true, .fixedDrive = true, .renamable = true};
        QCOMPARE(rendered(fe::sidebarMenu(system)),
                 (QStringList{QStringLiteral("Open"), QStringLiteral("Open in New Tab"), QStringLiteral("—"), QStringLiteral("Get Info"),
                              QStringLiteral("Rename “USB”")}));
        // Not mounted, so no Get Info: Rename stands on its own.
        const fe::SidebarPlace stick{.url = QUrl(), .ejectAllowed = true, .renamable = true};
        QCOMPARE(rendered(fe::sidebarMenu(stick)),
                 (QStringList{QStringLiteral("Open"), QStringLiteral("Open in New Tab"), QStringLiteral("—"), QStringLiteral("Rename “USB”"),
                              QStringLiteral("—"), QStringLiteral("Eject “USB”")}));
    }

    // The drives of the sidebar: their names, order, symbols and how full they are.
    void theSystemDiskIsNamedAfterTheComputerLikeMacintoshHd()
    {
        const fe::Drive system{.key = QStringLiteral("system"), .label = QStringLiteral("1.9 TB Internal Drive"), .kind = fe::DriveKind::System};
        QCOMPARE(fe::driveName(system, {}, QStringLiteral("XT0N1-PC")), QStringLiteral("XT0N1 PC"));
        QCOMPARE(fe::systemDriveName(QStringLiteral("my_box")), QStringLiteral("my box"));
        QCOMPARE(fe::systemDriveName(QString()), QStringLiteral("System"));
    }

    void aDriveShowsTheNameTheUserGaveIt()
    {
        const fe::Drive laptop{.key = QStringLiteral("uuid-1234"), .label = QStringLiteral("XT0N1 Laptop"), .kind = fe::DriveKind::Internal};
        const QMap<QString, QString> names{{QStringLiteral("uuid-1234"), QStringLiteral("Juegos SSD")}, {QStringLiteral("system"), QStringLiteral("  ")}};
        QCOMPARE(fe::driveName(laptop, names, QStringLiteral("XT0N1-PC")), QStringLiteral("Juegos SSD"));
        QCOMPARE(fe::driveName(laptop, {}, QStringLiteral("XT0N1-PC")), QStringLiteral("XT0N1 Laptop"));
        // A blank name counts as none.
        const fe::Drive system{.key = QStringLiteral("system"), .label = QString(), .kind = fe::DriveKind::System};
        QCOMPARE(fe::driveName(system, names, QStringLiteral("XT0N1-PC")), QStringLiteral("XT0N1 PC"));
    }

    void aTypedNameIsTidiedAndAnEmptyOneMeansTheDefault()
    {
        QCOMPARE(fe::cleanDriveName(QStringLiteral("  Mi   Disco \t Rápido ")), QStringLiteral("Mi Disco Rápido"));
        QCOMPARE(fe::cleanDriveName(QStringLiteral("   ")), QString());
        QCOMPARE(fe::cleanDriveName(QString(80, u'x')).size(), 64);
    }

    void drivesGoSystemFirstThenBuiltInThenRemovableEachByName()
    {
        const QList<fe::Drive> drives{{.key = QStringLiteral("dev-sda1"), .label = QStringLiteral("COS_202608"), .kind = fe::DriveKind::Stick},
                                      {.key = QStringLiteral("uuid-b"), .label = QStringLiteral("XT0N1 Laptop"), .kind = fe::DriveKind::Internal},
                                      {.key = QStringLiteral("system"), .label = QString(), .kind = fe::DriveKind::System},
                                      {.key = QStringLiteral("uuid-a"), .label = QStringLiteral("archive 10"), .kind = fe::DriveKind::Internal},
                                      {.key = QStringLiteral("uuid-c"), .label = QStringLiteral("Archive 9"), .kind = fe::DriveKind::Internal},
                                      {.key = QStringLiteral("path-/mnt/nas"), .label = QStringLiteral("Alpha NAS"), .kind = fe::DriveKind::Network},
                                      {.key = QStringLiteral("dev-sr0"), .label = QStringLiteral("Audio CD"), .kind = fe::DriveKind::Optical}};
        QStringList keys;
        for (const qsizetype i : fe::driveOrder(drives, {}, QStringLiteral("XT0N1-PC"))) {
            keys.append(drives.at(i).key);
        }
        // Numbers sort as numbers ("Archive 9" before "archive 10"), letters without case.
        QCOMPARE(keys, (QStringList{QStringLiteral("system"), QStringLiteral("uuid-c"), QStringLiteral("uuid-a"), QStringLiteral("uuid-b"),
                                    QStringLiteral("dev-sr0"), QStringLiteral("dev-sda1"), QStringLiteral("path-/mnt/nas")}));
    }

    void aRenamedDriveSortsByItsNewName()
    {
        const QList<fe::Drive> drives{{.key = QStringLiteral("uuid-a"), .label = QStringLiteral("Alpha"), .kind = fe::DriveKind::Internal},
                                      {.key = QStringLiteral("uuid-b"), .label = QStringLiteral("Beta"), .kind = fe::DriveKind::Internal}};
        const QList<qsizetype> order = fe::driveOrder(drives, {{QStringLiteral("uuid-a"), QStringLiteral("Zeta")}}, QString());
        QCOMPARE(order, (QList<qsizetype>{1, 0}));
    }

    void theCapacityBarWarnsWhenADriveIsNearlyFull()
    {
        const fe::Capacity roomy = fe::capacityFor(1000, 210);
        QCOMPARE(roomy.used, 0.79);
        QCOMPARE(roomy.level, fe::Capacity::Level::Roomy);
        QCOMPARE(fe::capacityFor(1000, 100).level, fe::Capacity::Level::Roomy); // exactly 90 % used
        QCOMPARE(fe::capacityFor(1000, 80).level, fe::Capacity::Level::Low);
        QCOMPARE(fe::capacityFor(1000, 50).level, fe::Capacity::Level::Low); // exactly 95 %
        QCOMPARE(fe::capacityFor(1000, 40).level, fe::Capacity::Level::Critical);
        // Unknown size: an empty bar, never a division by zero; more free than the size is clamped.
        QCOMPARE(fe::capacityFor(0, 0).used, 0.0);
        QCOMPARE(fe::capacityFor(100, 500).used, 0.0);
    }

    void theCapacityTextGivesFreeAndTotal()
    {
        const auto gb = [](qint64 bytes) { return QStringLiteral("%1 GB").arg(bytes / 1000000000); };
        QCOMPARE(fe::capacityText(1900000000000, 212000000000, gb), QStringLiteral("212 GB free of 1900 GB"));
    }

    void everyDriveKindHasItsOwnSymbol()
    {
        QSet<QString> symbols;
        for (const fe::DriveKind kind : {fe::DriveKind::System, fe::DriveKind::Internal, fe::DriveKind::External, fe::DriveKind::Stick,
                                         fe::DriveKind::Optical, fe::DriveKind::Card, fe::DriveKind::Network}) {
            const QString symbol = fe::driveSymbol(kind);
            QVERIFY2(symbol.startsWith(u":/icons/places/"), qPrintable(symbol));
            symbols.insert(symbol);
        }
        QCOMPARE(symbols.size(), 7);
    }

    void aDiscIsEjectedEvenBeforeItIsMounted()
    {
        const fe::SidebarPlace disc{.url = QUrl(), .ejectAllowed = true};
        QCOMPARE(rendered(fe::sidebarMenu(disc)),
                 (QStringList{QStringLiteral("Open"), QStringLiteral("Open in New Tab"), QStringLiteral("—"), QStringLiteral("Eject “USB”")}));
    }

    void aDriveThatCannotBeUnmountedNowOffersNeitherEjectNorUnmount()
    {
        const fe::SidebarPlace busy{.url = folder(QStringLiteral("/run/media/alex/USB"))};
        const QList<fe::SidebarCommand> commands = commandsIn(fe::sidebarMenu(busy));
        QVERIFY(!commands.contains(fe::SidebarCommand::Eject));
        QVERIFY(!commands.contains(fe::SidebarCommand::Unmount));
    }

    void placesWithoutALocalFolderHaveNoGetInfo()
    {
        for (const QUrl &url : {QUrl(QStringLiteral("recentlyused:/files")), QUrl(QStringLiteral("remote:/"))}) {
            QCOMPARE(rendered(fe::sidebarMenu({.url = url})), (QStringList{QStringLiteral("Open"), QStringLiteral("Open in New Tab")}));
        }
    }

    void theTrashOffersEmptyTrashDimmedWhileItIsEmpty()
    {
        const QUrl trash(QStringLiteral("trash:/"));
        QCOMPARE(rendered(fe::sidebarMenu({.url = trash, .trashEmpty = false})),
                 (QStringList{QStringLiteral("Open"), QStringLiteral("Open in New Tab"), QStringLiteral("—"), QStringLiteral("Empty Trash")}));
        QCOMPARE(rendered(fe::sidebarMenu({.url = trash, .trashEmpty = true})).constLast(), QStringLiteral("Empty Trash (dimmed)"));
    }

    void everySidebarMenuOpensFirstAndOffersOnlyWhatTheRowAllows()
    {
        const quint32 seed = QRandomGenerator::global()->generate();
        QRandomGenerator random(seed);
        const QList<QUrl> urls{folder(QStringLiteral("/home/alex/Projects")), folder(QStringLiteral("/")),
                               folder(QStringLiteral("/run/media/alex/USB")), QUrl(QStringLiteral("trash:/")),
                               QUrl(QStringLiteral("recentlyused:/files")), QUrl(QStringLiteral("remote:/")), QUrl()};
        const auto coin = [&random] { return random.bounded(2) == 1; };
        for (int run = 0; run < 2000; ++run) {
            const fe::SidebarPlace place{.url = urls.at(static_cast<int>(random.bounded(static_cast<int>(urls.size())))),
                                         .addedByUser = coin(), .ejectAllowed = coin(), .teardownAllowed = coin(),
                                         .fixedDrive = coin(), .trashEmpty = coin(), .renamable = coin()};
            const QList<QList<fe::SidebarMenuEntry>> menu = fe::sidebarMenu(place);
            const QList<fe::SidebarCommand> commands = commandsIn(menu);
            const QByteArray where = "seed " + QByteArray::number(seed) + ", run " + QByteArray::number(run) + ", " + place.url.toEncoded();

            QVERIFY2(commands.mid(0, 2) == (QList<fe::SidebarCommand>{fe::SidebarCommand::Open, fe::SidebarCommand::OpenInNewTab}), where.constData());
            QVERIFY2(menu.constFirst().size() == 2, where.constData());
            for (const QList<fe::SidebarMenuEntry> &group : menu) {
                QVERIFY2(!group.isEmpty(), where.constData());
            }
            for (const fe::SidebarCommand command : commands) {
                QVERIFY2(commands.count(command) == 1, where.constData());
            }
            QVERIFY2(commands.contains(fe::SidebarCommand::GetInfo) == place.url.isLocalFile(), where.constData());
            QVERIFY2(commands.contains(fe::SidebarCommand::RemoveFromSidebar) == place.addedByUser, where.constData());
            QVERIFY2(commands.contains(fe::SidebarCommand::Rename) == place.renamable, where.constData());
            QVERIFY2(commands.contains(fe::SidebarCommand::EmptyTrash) == (place.url.scheme() == u"trash"), where.constData());
            const bool drive = commands.contains(fe::SidebarCommand::Eject) || commands.contains(fe::SidebarCommand::Unmount);
            const bool systemDisk = place.url == folder(QStringLiteral("/"));
            QVERIFY2(drive == (place.ejectAllowed || (place.teardownAllowed && !systemDisk)), where.constData());
            QVERIFY2(!(commands.contains(fe::SidebarCommand::Eject) && commands.contains(fe::SidebarCommand::Unmount)), where.constData());
            if (commands.contains(fe::SidebarCommand::Unmount)) {
                QVERIFY2(place.fixedDrive && !place.ejectAllowed, where.constData());
            }
        }
    }

    void addToSidebarAddsTheSelectedFoldersRatherThanTheFolderAroundThem()
    {
        const QUrl mods = folder(QStringLiteral("/home/alex/Downloads/Mods"));
        const QUrl wallpapers = folder(QStringLiteral("/home/alex/Downloads/Wallpapers"));
        QCOMPARE(fe::foldersToAdd({mods, wallpapers}, folder(QStringLiteral("/home/alex/Downloads")), NOTHING_IN_SIDEBAR),
                 (QList<QUrl>{mods, wallpapers}));
    }

    void withNoFolderSelectedAddToSidebarAddsTheCurrentFolder()
    {
        const QUrl games = folder(QStringLiteral("/home/alex/Games"));
        QCOMPARE(fe::foldersToAdd({}, games, NOTHING_IN_SIDEBAR), QList<QUrl>{games});
    }

    void aFolderTheSidebarAlreadyShowsIsNotAddedAgain()
    {
        const QUrl downloads = folder(QStringLiteral("/home/alex/Downloads"));
        const QUrl mods = folder(QStringLiteral("/home/alex/Downloads/Mods"));
        const auto showsDownloads = [&downloads](const QUrl &url) { return url == downloads; };
        QCOMPARE(fe::foldersToAdd({downloads, mods}, QUrl(), showsDownloads), QList<QUrl>{mods});
        QCOMPARE(fe::foldersToAdd({}, downloads, showsDownloads), QList<QUrl>{});
    }

    void aFolderSelectedTwiceIsAddedOnceWithoutItsTrailingSlash()
    {
        const QUrl mods = folder(QStringLiteral("/home/alex/Downloads/Mods"));
        QCOMPARE(fe::foldersToAdd({folder(QStringLiteral("/home/alex/Downloads/Mods/")), mods}, QUrl(), NOTHING_IN_SIDEBAR), QList<QUrl>{mods});
    }

    void withoutAFolderNothingIsAdded()
    {
        QCOMPARE(fe::foldersToAdd({}, QUrl(), NOTHING_IN_SIDEBAR), QList<QUrl>{});
    }

    void addToSidebarNeverAddsWhatTheSidebarShowsNorTheSameFolderTwice()
    {
        const quint32 seed = QRandomGenerator::global()->generate();
        QRandomGenerator random(seed);
        QList<QUrl> pool;
        for (const char *name : {"Mods", "Wallpapers", "Games", "Music", "Projects", "Notes"}) {
            const QString path = QStringLiteral("/home/alex/") + QString::fromLatin1(name);
            pool.append(folder(path));
            pool.append(folder(path + u'/'));
        }
        for (int run = 0; run < 2000; ++run) {
            QList<QUrl> selected;
            const int count = static_cast<int>(random.bounded(5));
            for (int i = 0; i < count; ++i) {
                selected.append(pool.at(static_cast<int>(random.bounded(static_cast<int>(pool.size())))));
            }
            const QUrl current = pool.at(static_cast<int>(random.bounded(static_cast<int>(pool.size()))));
            const quint32 shownMask = random.bounded(64U);
            const auto inSidebar = [&shownMask, &pool](const QUrl &url) {
                for (int i = 0; i < pool.size(); i += 2) {
                    if ((shownMask & (1U << (i / 2))) && url.matches(pool.at(i), QUrl::StripTrailingSlash)) {
                        return true;
                    }
                }
                return false;
            };
            const QList<QUrl> added = fe::foldersToAdd(selected, current, inSidebar);
            const QList<QUrl> asked = selected.isEmpty() ? QList<QUrl>{current} : selected;
            const QByteArray where = "seed " + QByteArray::number(seed) + ", run " + QByteArray::number(run);

            for (const QUrl &url : added) {
                QVERIFY2(!inSidebar(url), where.constData());
                QVERIFY2(added.count(url) == 1, where.constData());
                QVERIFY2(!url.path().endsWith(u'/'), where.constData());
                QVERIFY2(std::any_of(asked.cbegin(), asked.cend(), [&url](const QUrl &a) { return a.matches(url, QUrl::StripTrailingSlash); }),
                         where.constData());
            }
            for (const QUrl &url : asked) {
                const bool kept = std::any_of(added.cbegin(), added.cend(), [&url](const QUrl &a) { return a.matches(url, QUrl::StripTrailingSlash); });
                QVERIFY2(kept == !inSidebar(url), where.constData());
            }
        }
    }

    void aPlaceIsNamedLikeItsFolder()
    {
        QCOMPARE(fe::placeTitle(folder(QStringLiteral("/home/alex/Downloads/Mods")), HOME_CONTEXT), QStringLiteral("Mods"));
        QCOMPARE(fe::placeTitle(folder(QStringLiteral("/")), HOME_CONTEXT), QStringLiteral("CachyOS"));
        QCOMPARE(fe::placeTitle(QUrl(QStringLiteral("sftp://nas/media/Movies/")), HOME_CONTEXT), QStringLiteral("Movies"));
        QCOMPARE(fe::placeTitle(QUrl(QStringLiteral("sftp://nas/")), HOME_CONTEXT), QStringLiteral("nas"));
    }

    void ejectAndUnmountNameTheDriveInCurlyQuotes()
    {
        QCOMPARE(fe::sidebarCommandText(fe::SidebarCommand::Eject, QStringLiteral("COS_202608")), QStringLiteral("Eject “COS_202608”"));
        QCOMPARE(fe::sidebarCommandText(fe::SidebarCommand::Unmount, QStringLiteral("XTON1 Laptop")), QStringLiteral("Unmount “XTON1 Laptop”"));
    }

    void filesDroppedOnAPlaceGoIntoIt()
    {
        QCOMPARE(fe::sidebarDrop(14, 28, true, true, false), fe::SidebarDrop::IntoPlace);
        // Even at a row's edge: only folders can join the sidebar.
        QCOMPARE(fe::sidebarDrop(2, 28, true, true, false), fe::SidebarDrop::IntoPlace);
    }

    void foldersDroppedNearTheEdgeOfAFavoriteJoinTheSidebarThere()
    {
        QCOMPARE(fe::sidebarDrop(3, 28, true, true, true), fe::SidebarDrop::BeforeRow);
        QCOMPARE(fe::sidebarDrop(25, 28, true, true, true), fe::SidebarDrop::AfterRow);
    }

    void foldersDroppedOnTheMiddleOfAFavoriteGoIntoIt()
    {
        QCOMPARE(fe::sidebarDrop(14, 28, true, true, true), fe::SidebarDrop::IntoPlace);
    }

    void foldersNeverJoinLocationsTheyGoIntoTheDrive()
    {
        QCOMPARE(fe::sidebarDrop(2, 28, false, true, true), fe::SidebarDrop::IntoPlace);
        QCOMPARE(fe::sidebarDrop(26, 28, false, true, true), fe::SidebarDrop::IntoPlace);
    }

    void aFavoriteThatTakesNoFilesTakesFoldersAboveOrBelowIt()
    {
        // Recents holds no files of its own.
        QCOMPARE(fe::sidebarDrop(10, 28, true, false, true), fe::SidebarDrop::BeforeRow);
        QCOMPARE(fe::sidebarDrop(18, 28, true, false, true), fe::SidebarDrop::AfterRow);
        QCOMPARE(fe::sidebarDrop(14, 28, true, false, false), fe::SidebarDrop::Nowhere);
    }

    void aLocationThatTakesNoFilesTakesNothing()
    {
        // Network.
        QCOMPARE(fe::sidebarDrop(14, 28, false, false, true), fe::SidebarDrop::Nowhere);
    }

    void everyDropLandsOnlyWhereTheRowAllows()
    {
        const quint32 seed = QRandomGenerator::global()->generate();
        QRandomGenerator random(seed);
        for (int run = 0; run < 5000; ++run) {
            const qreal height = 20 + random.bounded(20);
            const qreal y = random.bounded(height);
            const bool inFavorites = random.bounded(2) == 1;
            const bool acceptsFiles = random.bounded(2) == 1;
            const bool onlyFolders = random.bounded(2) == 1;
            const fe::SidebarDrop drop = fe::sidebarDrop(y, height, inFavorites, acceptsFiles, onlyFolders);
            const bool joins = drop == fe::SidebarDrop::BeforeRow || drop == fe::SidebarDrop::AfterRow;
            const bool canJoin = inFavorites && onlyFolders;
            const QByteArray where = "seed " + QByteArray::number(seed) + ", run " + QByteArray::number(run);

            QVERIFY2(!joins || canJoin, where.constData());
            QVERIFY2(drop != fe::SidebarDrop::IntoPlace || acceptsFiles, where.constData());
            QVERIFY2((drop == fe::SidebarDrop::Nowhere) == (!acceptsFiles && !canJoin), where.constData());
            // Above a row always means its upper half, below it its lower half.
            QVERIFY2(drop != fe::SidebarDrop::BeforeRow || y < height / 2, where.constData());
            QVERIFY2(drop != fe::SidebarDrop::AfterRow || y >= height / 2, where.constData());
            // The quarter at each edge of a Favorites row always takes folders into the sidebar.
            if (canJoin && (y < height / 4 || y >= height - height / 4)) {
                QVERIFY2(joins, where.constData());
            }
        }
    }

    void aSectionKeepsItsOwnOrderUntilTheUserMovesARow()
    {
        const QStringList keys{QStringLiteral("recents"), QStringLiteral("desktop"), QStringLiteral("documents")};
        QCOMPARE(fe::sidebarOrder(keys, {}), (QList<qsizetype>{0, 1, 2}));
    }

    void theUsersOrderWinsAndNewPlacesFollowIt()
    {
        const QStringList keys{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("d")};
        // "gone" is a place that left the sidebar; b and d are new since the order was saved.
        const QStringList saved{QStringLiteral("c"), QStringLiteral("gone"), QStringLiteral("a")};
        QCOMPARE(fe::sidebarOrder(keys, saved), (QList<qsizetype>{2, 0, 1, 3}));
    }

    void aDraggedRowLandsAboveOrBelowTheRowItIsDroppedOn()
    {
        const QStringList keys{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("d")};
        QCOMPARE(fe::movedSidebarRow(keys, 0, 2, true), (QStringList{QStringLiteral("b"), QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("d")}));
        QCOMPARE(fe::movedSidebarRow(keys, 3, 0, false), (QStringList{QStringLiteral("d"), QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));
        QCOMPARE(fe::movedSidebarRow(keys, 1, 2, false), keys);
        QCOMPARE(fe::movedSidebarRow(keys, 1, 1, true), keys);
    }

    void aDraggedRowGoesAboveInTheUpperHalfAndBelowInTheLowerHalf()
    {
        QCOMPARE(fe::sidebarRowDrop(5, 28, true), fe::SidebarDrop::BeforeRow);
        QCOMPARE(fe::sidebarRowDrop(20, 28, true), fe::SidebarDrop::AfterRow);
    }

    void aRowNeverMovesToTheOtherSection()
    {
        QCOMPARE(fe::sidebarRowDrop(5, 28, false), fe::SidebarDrop::Nowhere);
    }

    void movingARowKeepsEveryRowOnceAndPutsItNextToItsTarget()
    {
        const quint32 seed = QRandomGenerator::global()->generate();
        QRandomGenerator random(seed);
        for (int run = 0; run < 5000; ++run) {
            QStringList keys;
            const int count = 1 + static_cast<int>(random.bounded(8));
            for (int i = 0; i < count; ++i) {
                keys.append(QString::number(i));
            }
            const qsizetype from = random.bounded(count);
            const qsizetype to = random.bounded(count);
            const bool below = random.bounded(2) == 1;
            const QStringList moved = fe::movedSidebarRow(keys, from, to, below);
            const QByteArray where = "seed " + QByteArray::number(seed) + ", run " + QByteArray::number(run);

            QStringList sorted = moved;
            std::sort(sorted.begin(), sorted.end(), [](const QString &a, const QString &b) { return a.toInt() < b.toInt(); });
            QVERIFY2(sorted == keys, where.constData());
            if (from != to) {
                const qsizetype at = moved.indexOf(keys.at(from));
                const qsizetype target = moved.indexOf(keys.at(to));
                QVERIFY2(at == (below ? target + 1 : target - 1), where.constData());
            }
            // The saved order puts the rows back the way the move left them.
            QList<qsizetype> expected;
            for (const QString &key : moved) {
                expected.append(keys.indexOf(key));
            }
            QVERIFY2(fe::sidebarOrder(keys, moved) == expected, where.constData());
        }
    }

    // Every transition follows the desktop's animation speed (kdeglobals [KDE] AnimationDurationFactor): it scales
    // the base time, and 0 turns animations off, as everywhere in Plasma.
    void a_duration_follows_the_desktop_s_animation_speed()
    {
        QCOMPARE(fe::motion::scaled(150, 1.0), 150);
        QCOMPARE(fe::motion::scaled(150, 0.5), 75);
        QCOMPARE(fe::motion::scaled(150, 1.5), 225);
        QCOMPARE(fe::motion::scaled(150, 0.0), 0);
        QCOMPARE(fe::motion::scaled(150, -1.0), 0);
    }

    void a_hover_fades_in_and_out_instead_of_switching()
    {
        fe::motion::HoverFade fade(150, 1.0);
        int changes = 0;
        connect(&fade, &fe::motion::HoverFade::changed, this, [&changes] { ++changes; });
        fade.setHovered(true);
        QVERIFY(fade.progress() < 1.0);
        QTRY_COMPARE_WITH_TIMEOUT(fade.progress(), 1.0, 1000);
        QVERIFY2(changes > 2, "the widget is told on every frame, so it repaints the fade");
        fade.setHovered(false);
        QTRY_COMPARE_WITH_TIMEOUT(fade.progress(), 0.0, 1000);
    }

    void instant_animations_switch_the_hover_at_once()
    {
        fe::motion::HoverFade fade(150, 0.0);
        fade.setHovered(true);
        QCOMPARE(fade.progress(), 1.0);
        fade.setHovered(false);
        QCOMPARE(fade.progress(), 0.0);
    }

    // Tabs and the window buttons have one hover for several segments: the next segment fades in from nothing.
    void a_hover_moved_to_another_segment_fades_in_from_nothing()
    {
        fe::motion::HoverFade fade(150, 1.0);
        fade.setHovered(true);
        QTRY_COMPARE_WITH_TIMEOUT(fade.progress(), 1.0, 1000);
        fade.reset();
        QCOMPARE(fade.progress(), 0.0);
        fade.setHovered(true);
        QVERIFY(fade.progress() < 1.0);
        QTRY_COMPARE_WITH_TIMEOUT(fade.progress(), 1.0, 1000);
    }

    void a_hover_that_turns_around_halfway_fades_back_from_where_it_is()
    {
        fe::motion::HoverFade fade(400, 1.0);
        fade.setHovered(true);
        QTRY_VERIFY_WITH_TIMEOUT(fade.progress() > 0.3, 1000);
        const qreal reached = fade.progress();
        fade.setHovered(false);
        QVERIFY2(fade.progress() <= reached && fade.progress() > 0.0, "no jump back to fully hovered or to none");
    }

    void a_filter_counts_its_matches_in_words()
    {
        QCOMPARE(fe::filterMatchText(0), QStringLiteral("No matches"));
        QCOMPARE(fe::filterMatchText(1), QStringLiteral("1 match"));
        QCOMPARE(fe::filterMatchText(1204), QStringLiteral("1204 matches"));
    }

    void copy_path_writes_every_item_one_per_line()
    {
        QCOMPARE(fe::copiedPaths({folder(QStringLiteral("/home/alex/a b.txt"))}), QStringLiteral("/home/alex/a b.txt"));
        QCOMPARE(fe::copiedPaths({folder(QStringLiteral("/home/alex/Ñandú")), folder(QStringLiteral("/home/alex/Games")),
                                  QUrl(QStringLiteral("smb://nas/share/doc%20one.pdf"))}),
                 QStringLiteral("/home/alex/Ñandú\n/home/alex/Games\nsmb://nas/share/doc one.pdf"));
        QCOMPARE(fe::copiedPaths({}), QString());
    }

    void a_palette_query_finds_letters_in_order_without_case()
    {
        QVERIFY(fe::fuzzyScore(u"nf", u"New Folder").has_value());
        QVERIFY(fe::fuzzyScore(u"NEWF", u"New Folder").has_value());
        QVERIFY(!fe::fuzzyScore(u"fn", u"New Folder").has_value());
        QVERIFY(!fe::fuzzyScore(u"x", u"New Folder").has_value());
        QCOMPARE(fe::fuzzyScore(u"", u"New Folder"), std::optional<int>(0));
    }

    void a_palette_prefers_word_starts_and_runs_of_letters()
    {
        // Word starts beat letters inside words; the start of the text beats both; a run beats scattered letters.
        QVERIFY(*fe::fuzzyScore(u"nf", u"New Folder") > *fe::fuzzyScore(u"nf", u"Unfold"));
        QVERIFY(*fe::fuzzyScore(u"re", u"Reload") > *fe::fuzzyScore(u"re", u"Show Previous Tab"));
        QVERIFY(*fe::fuzzyScore(u"split", u"Split View") > *fe::fuzzyScore(u"split", u"xsxpxlxixt"));
        // A capital inside a word starts a word too.
        QVERIFY(*fe::fuzzyScore(u"ql", u"QuickLook") > *fe::fuzzyScore(u"ql", u"Quell"));
    }

    void the_palette_ranks_best_first_and_keeps_ties_in_order()
    {
        const QList<fe::PaletteEntry> entries{
            {QStringLiteral("New Tab"), QStringLiteral("File")},
            {QStringLiteral("New Folder"), QStringLiteral("File")},
            {QStringLiteral("as List"), QStringLiteral("View")},
            {QStringLiteral("Show Hidden Files"), QStringLiteral("View")},
            {QStringLiteral("Close Window"), QStringLiteral("File")},
        };
        // "Show Hidden Files" holds n and f too, inside words, so it comes after.
        QCOMPARE(fe::rankPalette(QStringLiteral("nf"), entries), QList<qsizetype>({1, 3}));
        QCOMPARE(fe::rankPalette(QStringLiteral("new"), entries), QList<qsizetype>({0, 1}));
        // The place helps find a command whose text alone does not hold the letters.
        QCOMPARE(fe::rankPalette(QStringLiteral("view list"), entries).value(0), 2);
        QCOMPARE(fe::rankPalette(QStringLiteral("hidden"), entries), QList<qsizetype>({3}));
        QCOMPARE(fe::rankPalette(QString(), entries).size(), entries.size());
        QVERIFY(fe::rankPalette(QStringLiteral("zzz"), entries).isEmpty());
    }

};

QTEST_GUILESS_MAIN(CoreTest)
#include "test_core.moc"
