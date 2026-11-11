// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Unit tests for the widget-free logic of the Finder features: the typed address, the path segments, color tags, the
// sidebar's folder tree and Quick Look.

#include "core/address.h"
#include "core/crumbs.h"
#include "core/paths.h"
#include "core/quicklook.h"
#include "core/tags.h"
#include "core/tree.h"

#include <QHash>
#include <QUrlQuery>
#include <QSet>
#include <QTest>

namespace {

const QString HOME = QStringLiteral("/home/alex");
const QUrl GAMES = QUrl::fromLocalFile(QStringLiteral("/home/alex/Games"));

// Folders on a pretend disk: each path names its subfolders.
const QHash<QString, QStringList> DISK{
    {QStringLiteral("/mnt/D/X"), {QStringLiteral("2-Dev"), QStringLiteral("Music")}},
    {QStringLiteral("/mnt/D/X/2-Dev"), {QStringLiteral("Aurora"), QStringLiteral("Orion")}},
    {QStringLiteral("/mnt/D/X/2-Dev/Aurora"), {QStringLiteral("docs")}},
    {QStringLiteral("/mnt/D/X/Music"), {}},
};

QUrl local(const char *path)
{
    return QUrl::fromLocalFile(QString::fromUtf8(path));
}

const QString TAGS = QStringLiteral("/home/alex/.local/share/file-explorer/tags");

/// The crumbs a layout shows, left to right; -1 is the ellipsis.
QList<qsizetype> shownCrumbs(const fe::CrumbLayout &layout)
{
    QList<qsizetype> out;
    for (const fe::CrumbSlot &slot : layout.cells) {
        out.append(slot.crumb);
    }
    return out;
}

QStringList subfoldersOf(const QString &path)
{
    return DISK.value(path);
}

QStringList paths(const QList<fe::TreeEntry> &rows)
{
    QStringList out;
    for (const fe::TreeEntry &row : rows) {
        out.append(QString(row.depth * 2, u' ') + row.path);
    }
    return out;
}

} // namespace

class FinderTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theAddressOfALocalFolderIsItsPath()
    {
        QCOMPARE(fe::addressText(QUrl::fromLocalFile(QStringLiteral("/mnt/D/X/2-Dev"))), QStringLiteral("/mnt/D/X/2-Dev"));
        QCOMPARE(fe::addressText(QUrl::fromLocalFile(QStringLiteral("/"))), QStringLiteral("/"));
    }

    void theAddressOfAnotherPlaceIsItsUrl()
    {
        QCOMPARE(fe::addressText(QUrl(QStringLiteral("trash:/"))), QStringLiteral("trash:/"));
        QCOMPARE(fe::addressText(QUrl(QStringLiteral("smb://nas/share"))), QStringLiteral("smb://nas/share"));
    }

    void whileSearchingTheAddressIsTheFolderSearched()
    {
        const QUrl search = fe::filenameSearchUrl(QStringLiteral("mods"), GAMES, QStringLiteral("Search in Games"));
        QCOMPARE(fe::addressText(search), QStringLiteral("/home/alex/Games"));
    }

    void anIndexedSearchCarriesItsWordsScopeAndTitle()
    {
        const QUrl everywhere = fe::indexedSearchUrl(QStringLiteral("a&b=c d"), QUrl(), QStringLiteral("Searching"));
        QCOMPARE(everywhere.scheme(), QStringLiteral("aurora-search"));
        QCOMPARE(QUrlQuery(everywhere).queryItemValue(QStringLiteral("q"), QUrl::FullyDecoded), QStringLiteral("a&b=c d"));
        QVERIFY(!QUrlQuery(everywhere).hasQueryItem(QStringLiteral("scope")));
        const QUrl here = fe::indexedSearchUrl(QStringLiteral("x"), GAMES, QStringLiteral("Searching"));
        QCOMPARE(QUrlQuery(here).queryItemValue(QStringLiteral("scope"), QUrl::FullyDecoded), QStringLiteral("/home/alex/Games"));
        const fe::PathContext context{HOME, QStringLiteral("alex"), QStringLiteral("CachyOS")};
        QCOMPARE(fe::folderTitle(here, context), QStringLiteral("Searching"));
        QCOMPARE(fe::crumbDrop(here, true, {local("/tmp/x")}, TAGS), fe::CrumbDrop::Refused);
    }

    void aTagsShelfReadsAsTagsThenTheTag()
    {
        const fe::PathContext context{HOME, QStringLiteral("alex"), QStringLiteral("CachyOS"),
                                      QStringLiteral("/home/alex/.local/share/file-explorer/tags")};
        const QUrl red = QUrl::fromLocalFile(QStringLiteral("/home/alex/.local/share/file-explorer/tags/Red"));
        const QList<fe::Crumb> crumbs = fe::crumbsFor(red, context);
        QCOMPARE(crumbs.size(), 2);
        QCOMPARE(crumbs.at(0).label, QStringLiteral("Tags"));
        QCOMPARE(crumbs.at(1).label, QStringLiteral("Red"));
        QCOMPARE(crumbs.at(1).url, red);
        QCOMPARE(fe::folderTitle(red, context), QStringLiteral("Red"));
    }

    void withoutAShelfAFolderOfThatNameIsJustAFolder()
    {
        const fe::PathContext context{HOME, QStringLiteral("alex"), QStringLiteral("CachyOS")};
        const QUrl red = QUrl::fromLocalFile(QStringLiteral("/home/alex/.local/share/file-explorer/tags/Red"));
        QCOMPARE(fe::crumbsFor(red, context).constFirst().label, QStringLiteral("alex"));
    }

    void aTypedAbsolutePathOpensThatFolder()
    {
        QCOMPARE(fe::typedLocation(QStringLiteral("/mnt/D/X"), HOME, GAMES), QUrl::fromLocalFile(QStringLiteral("/mnt/D/X")));
        QCOMPARE(fe::typedLocation(QStringLiteral("  /mnt/D/X/  "), HOME, GAMES), QUrl::fromLocalFile(QStringLiteral("/mnt/D/X")));
    }

    void aTildeStartsAtHome()
    {
        QCOMPARE(fe::typedLocation(QStringLiteral("~"), HOME, GAMES), QUrl::fromLocalFile(HOME));
        QCOMPARE(fe::typedLocation(QStringLiteral("~/Downloads"), HOME, GAMES), QUrl::fromLocalFile(QStringLiteral("/home/alex/Downloads")));
    }

    void aRelativePathStartsAtTheFolderOnScreen()
    {
        QCOMPARE(fe::typedLocation(QStringLiteral("Mods"), HOME, GAMES), QUrl::fromLocalFile(QStringLiteral("/home/alex/Games/Mods")));
        QCOMPARE(fe::typedLocation(QStringLiteral(".."), HOME, GAMES), QUrl::fromLocalFile(HOME));
        QCOMPARE(fe::typedLocation(QStringLiteral("../Music/./Rock"), HOME, GAMES),
                 QUrl::fromLocalFile(QStringLiteral("/home/alex/Music/Rock")));
    }

    void aUrlWithASchemeIsTakenAsWritten()
    {
        QCOMPARE(fe::typedLocation(QStringLiteral("smb://nas/share"), HOME, GAMES), QUrl(QStringLiteral("smb://nas/share")));
        QCOMPARE(fe::typedLocation(QStringLiteral("trash:/"), HOME, GAMES), QUrl(QStringLiteral("trash:/")));
        QCOMPARE(fe::typedLocation(QStringLiteral("file:///etc"), HOME, GAMES), QUrl::fromLocalFile(QStringLiteral("/etc")));
    }

    void nothingTypedGoesNowhere()
    {
        QVERIFY(!fe::typedLocation(QStringLiteral("   "), HOME, GAMES).isValid());
    }

    void suggestionsAreTheFoldersThatStartWithWhatWasTyped()
    {
        const QStringList names{QStringLiteral("Music"), QStringLiteral("2-Dev"), QStringLiteral("mods"), QStringLiteral(".cache")};
        QCOMPARE(fe::addressSuggestions(QStringLiteral("/mnt/D/X/m"), names, 10),
                 (QStringList{QStringLiteral("/mnt/D/X/mods/"), QStringLiteral("/mnt/D/X/Music/")}));
        QCOMPARE(fe::addressSuggestions(QStringLiteral("/mnt/D/X/"), names, 10),
                 (QStringList{QStringLiteral("/mnt/D/X/2-Dev/"), QStringLiteral("/mnt/D/X/mods/"), QStringLiteral("/mnt/D/X/Music/")}));
    }

    void hiddenFoldersAreSuggestedOnlyAfterADot()
    {
        const QStringList names{QStringLiteral(".cache"), QStringLiteral(".config"), QStringLiteral("Code")};
        QCOMPARE(fe::addressSuggestions(QStringLiteral("~/.c"), names, 10),
                 (QStringList{QStringLiteral("~/.cache/"), QStringLiteral("~/.config/")}));
    }

    void suggestionsStopAtTheLimit()
    {
        QStringList names;
        for (int i = 1; i <= 30; ++i) {
            names.append(QStringLiteral("Folder %1").arg(i));
        }
        const QStringList shown = fe::addressSuggestions(QStringLiteral("/data/F"), names, 8);
        QCOMPARE(shown.size(), 8);
        // Numbers sort the way people count: 2 before 10.
        QCOMPARE(shown.at(1), QStringLiteral("/data/Folder 2/"));
    }

    void tabCompletesTheOnlyMatchingFolder()
    {
        QCOMPARE(fe::tabCompleted(QStringLiteral("/mnt/D/X/2-d"), {QStringLiteral("2-Dev"), QStringLiteral("Music")}),
                 QStringLiteral("/mnt/D/X/2-Dev/"));
    }

    void tabCompletesAsFarAsTheMatchesAgree()
    {
        QCOMPARE(fe::tabCompleted(QStringLiteral("~/d"), {QStringLiteral("Documents"), QStringLiteral("Downloads"), QStringLiteral("Desktop")}),
                 QStringLiteral("~/D"));
        QCOMPARE(fe::tabCompleted(QStringLiteral("~/Do"), {QStringLiteral("Documents"), QStringLiteral("Downloads")}), QStringLiteral("~/Do"));
        QCOMPARE(fe::tabCompleted(QStringLiteral("~/Dow"), {QStringLiteral("Documents"), QStringLiteral("Downloads")}),
                 QStringLiteral("~/Downloads/"));
    }

    void tabWithoutAMatchChangesNothing()
    {
        QCOMPARE(fe::tabCompleted(QStringLiteral("~/zz"), {QStringLiteral("Documents")}), QStringLiteral("~/zz"));
    }

    void everySuggestionCompletesToItself()
    {
        // Metamorphic: Tab on a full suggestion keeps it, whatever the folder holds.
        const QStringList names{QStringLiteral("alpha"), QStringLiteral("alphabet"), QStringLiteral("Beta"), QStringLiteral("beta 2")};
        for (const QString &suggestion : fe::addressSuggestions(QStringLiteral("/srv/"), names, 50)) {
            QCOMPARE(fe::tabCompleted(suggestion, names), suggestion);
        }
    }

    void finderHasSevenColorTagsInItsOrder()
    {
        QStringList names;
        for (const fe::ColorTag &tag : fe::colorTags()) {
            names.append(tag.name);
        }
        QCOMPARE(names, (QStringList{QStringLiteral("Red"), QStringLiteral("Orange"), QStringLiteral("Yellow"), QStringLiteral("Green"),
                                     QStringLiteral("Blue"), QStringLiteral("Purple"), QStringLiteral("Gray")}));
        // macOS's dark system blue.
        QCOMPARE(fe::colorTag(QStringLiteral("blue"))->color, QColor(10, 132, 255));
        QVERIFY(!fe::colorTag(QStringLiteral("work")).has_value());
    }

    void taggingAddsATagOnceAndKeepsTheOthers()
    {
        const QStringList tags{QStringLiteral("work"), QStringLiteral("Blue")};
        QCOMPARE(fe::taggedWith(tags, QStringLiteral("Red"), true),
                 (QStringList{QStringLiteral("work"), QStringLiteral("Blue"), QStringLiteral("Red")}));
        QCOMPARE(fe::taggedWith(tags, QStringLiteral("blue"), true), tags);
        QCOMPARE(fe::taggedWith(tags, QStringLiteral("BLUE"), false), (QStringList{QStringLiteral("work")}));
        QCOMPARE(fe::taggedWith(tags, QStringLiteral("Green"), false), tags);
    }

    void taggingTwiceIsLikeTaggingOnce()
    {
        const QStringList tags{QStringLiteral("Red")};
        for (const bool on : {true, false}) {
            const QStringList once = fe::taggedWith(tags, QStringLiteral("Purple"), on);
            QCOMPARE(fe::taggedWith(once, QStringLiteral("Purple"), on), once);
        }
    }

    void theColorTagsOfAFileComeInFindersOrder()
    {
        QCOMPARE(fe::colorTagNames({QStringLiteral("blue"), QStringLiteral("work"), QStringLiteral("Red")}),
                 (QStringList{QStringLiteral("Red"), QStringLiteral("Blue")}));
    }

    void aTaggedFileShowsAtMostThreeDots()
    {
        QCOMPARE(fe::tagEmblem({QStringLiteral("work")}), QString());
        QCOMPARE(fe::tagEmblem({QStringLiteral("Green")}), QStringLiteral("emblem-aurora-tags-green"));
        QCOMPARE(fe::tagEmblem({QStringLiteral("Gray"), QStringLiteral("Blue"), QStringLiteral("Red"), QStringLiteral("Yellow")}),
                 QStringLiteral("emblem-aurora-tags-red-yellow-blue"));
    }

    void everyEmblemIsOneTheThemeShips()
    {
        // The icon theme ships one emblem per set of one to three colors, named like this.
        const QStringList all = fe::tagEmblemNames();
        QCOMPARE(all.size(), 7 + 21 + 35);
        QCOMPARE(QSet<QString>(all.cbegin(), all.cend()).size(), all.size());
        QVERIFY(all.contains(fe::tagEmblem({QStringLiteral("Purple"), QStringLiteral("Orange")})));
    }

    void aShelfEntryKeepsTheFileNameUnlessItIsTaken()
    {
        QCOMPARE(fe::shelfEntryName(QStringLiteral("get.sh"), {}), QStringLiteral("get.sh"));
        QCOMPARE(fe::shelfEntryName(QStringLiteral("get.sh"), {QStringLiteral("get.sh")}), QStringLiteral("get 2.sh"));
        QCOMPARE(fe::shelfEntryName(QStringLiteral("get.sh"), {QStringLiteral("get.sh"), QStringLiteral("get 2.sh")}),
                 QStringLiteral("get 3.sh"));
        QCOMPARE(fe::shelfEntryName(QStringLiteral(".bashrc"), {QStringLiteral(".bashrc")}), QStringLiteral(".bashrc 2"));
        QCOMPARE(fe::shelfEntryName(QStringLiteral("Music"), {QStringLiteral("Music")}), QStringLiteral("Music 2"));
    }

    void aCollapsedFolderShowsNothingUnderIt()
    {
        QVERIFY(fe::expandedRows(QStringLiteral("/mnt/D/X"), {}, subfoldersOf).isEmpty());
    }

    void anExpandedFolderShowsItsSubfoldersAndTheirsWhileExpanded()
    {
        const QSet<QString> expanded{QStringLiteral("/mnt/D/X"), QStringLiteral("/mnt/D/X/2-Dev")};
        QCOMPARE(paths(fe::expandedRows(QStringLiteral("/mnt/D/X"), expanded, subfoldersOf)),
                 (QStringList{QStringLiteral("  /mnt/D/X/2-Dev"), QStringLiteral("    /mnt/D/X/2-Dev/Aurora"),
                              QStringLiteral("    /mnt/D/X/2-Dev/Orion"), QStringLiteral("  /mnt/D/X/Music")}));
    }

    void aFolderUnderACollapsedOneStaysHiddenEvenIfExpanded()
    {
        const QSet<QString> expanded{QStringLiteral("/mnt/D/X/2-Dev")};
        QVERIFY(fe::expandedRows(QStringLiteral("/mnt/D/X"), expanded, subfoldersOf).isEmpty());
    }

    void rowsKnowWhetherTheyAreExpanded()
    {
        const QSet<QString> expanded{QStringLiteral("/mnt/D/X"), QStringLiteral("/mnt/D/X/2-Dev")};
        const QList<fe::TreeEntry> rows = fe::expandedRows(QStringLiteral("/mnt/D/X"), expanded, subfoldersOf);
        QVERIFY(rows.at(0).expanded);
        QCOMPARE(rows.at(0).name, QStringLiteral("2-Dev"));
        QVERIFY(!rows.at(3).expanded);
    }

    void aFolderThatContainsItselfStopsAtTheDepthLimit()
    {
        // A symlink loop: every folder holds "loop", which is itself.
        const auto loop = [](const QString &) { return QStringList{QStringLiteral("loop")}; };
        QSet<QString> expanded;
        QString path = QStringLiteral("/l");
        for (int i = 0; i < 40; ++i) {
            expanded.insert(path);
            path += QStringLiteral("/loop");
        }
        QCOMPARE(fe::expandedRows(QStringLiteral("/l"), expanded, loop).size(), fe::TREE_DEPTH_LIMIT);
    }

    void imagesAndTextAreShownWholeEverythingElseAsAPreview()
    {
        QCOMPARE(fe::lookKindFor({QStringLiteral("image/png")}), fe::LookKind::Image);
        QCOMPARE(fe::lookKindFor({QStringLiteral("application/x-shellscript"), QStringLiteral("text/plain")}), fe::LookKind::Text);
        QCOMPARE(fe::lookKindFor({QStringLiteral("application/json")}), fe::LookKind::Text);
        QCOMPARE(fe::lookKindFor({QStringLiteral("application/pdf")}), fe::LookKind::Preview);
        QCOMPARE(fe::lookKindFor({QStringLiteral("inode/directory")}), fe::LookKind::Preview);
    }

    void spaceOpensQuickLookUnlessItContinuesATypedName()
    {
        // Dolphin adds a Space to a name being typed within 1000 ms of the last key
        // (kitemlistkeyboardsearchmanager.cpp, v26.08.2, lines 13 and 41-45), and Ctrl+Space toggles the selection
        // (kitemlistcontroller.cpp, lines 456-462).
        QVERIFY(fe::spaceShowsQuickLook(Qt::NoModifier, -1));
        QVERIFY(fe::spaceShowsQuickLook(Qt::NoModifier, 1500));
        QVERIFY(!fe::spaceShowsQuickLook(Qt::NoModifier, 400));
        QVERIFY(!fe::spaceShowsQuickLook(Qt::ControlModifier, -1));
        QVERIFY(!fe::spaceShowsQuickLook(Qt::ShiftModifier, -1));
    }

    void segmentsThatFitSitSideBySideWithASeparatorBetween()
    {
        const fe::CrumbLayout layout = fe::layoutCrumbs({40, 50, 60}, 14, 16, 12, 1000);
        QCOMPARE(shownCrumbs(layout), (QList<qsizetype>{0, 1, 2}));
        QCOMPARE(layout.cells.at(0).x, 14.0);
        QCOMPARE(layout.cells.at(1).x, 70.0);
        QCOMPARE(layout.cells.at(2).x, 136.0);
        QCOMPARE(layout.cells.at(2).width, 60.0);
        QVERIFY(layout.folded.isEmpty());
    }

    void aLongPathFoldsItsMiddleIntoAnEllipsisOutermostFirst()
    {
        // 40 + 50 + 60 + 70 + 80 and four separators of 10 is 340; folding the first two middle segments leaves
        // 40 + 12 + 70 + 80 and three separators, 232, inside 250.
        const fe::CrumbLayout layout = fe::layoutCrumbs({40, 50, 60, 70, 80}, 0, 10, 12, 250);
        QCOMPARE(shownCrumbs(layout), (QList<qsizetype>{0, -1, 3, 4}));
        QCOMPARE(layout.folded, (QList<qsizetype>{1, 2}));
        QCOMPARE(layout.cells.at(1).x, 50.0);
        QCOMPARE(layout.cells.at(1).width, 12.0);
        QCOMPARE(layout.cells.at(2).x, 72.0);
        QCOMPARE(layout.cells.at(3).x, 152.0);
    }

    void threeSegmentsFoldTheMiddleOneToo()
    {
        const fe::CrumbLayout layout = fe::layoutCrumbs({40, 300, 60}, 0, 10, 12, 150);
        QCOMPARE(shownCrumbs(layout), (QList<qsizetype>{0, -1, 2}));
        QCOMPARE(layout.folded, (QList<qsizetype>{1}));
    }

    void theLastSegmentShrinksWhenEvenTheFoldedPathIsTooWide()
    {
        // The first segment, the ellipsis and two separators take 72 of 200; the folder on screen gets the rest.
        fe::CrumbLayout layout = fe::layoutCrumbs({40, 50, 300}, 0, 10, 12, 200);
        QCOMPARE(shownCrumbs(layout), (QList<qsizetype>{0, -1, 2}));
        QCOMPARE(layout.cells.last().width, 128.0);
        layout = fe::layoutCrumbs({40, 50, 300}, 0, 10, 12, 30);
        QCOMPARE(layout.cells.last().width, 0.0);
    }

    void aSingleSegmentNeverFoldsItShrinks()
    {
        const fe::CrumbLayout layout = fe::layoutCrumbs({500}, 6, 10, 12, 100);
        QCOMPARE(shownCrumbs(layout), (QList<qsizetype>{0}));
        QCOMPARE(layout.cells.at(0).width, 100.0);
        QVERIFY(fe::layoutCrumbs({}, 0, 10, 12, 100).cells.isEmpty());
    }

    void aPointerFindsTheLabelOrTheChevronAfterIt()
    {
        // Slots at 0-40, 56-106 and 122-182; the chevrons sit in the 16 px between them.
        const fe::CrumbLayout layout = fe::layoutCrumbs({40, 50, 60}, 0, 16, 12, 1000);
        QCOMPARE(fe::crumbHitAt(layout, 20, 16, 3).slot, 0);
        QCOMPARE(fe::crumbHitAt(layout, 20, 16, 3).part, fe::CrumbPart::Label);
        QCOMPARE(fe::crumbHitAt(layout, 42, 16, 3).part, fe::CrumbPart::Label);
        QCOMPARE(fe::crumbHitAt(layout, 48, 16, 3).slot, 0);
        QCOMPARE(fe::crumbHitAt(layout, 48, 16, 3).part, fe::CrumbPart::Chevron);
        QCOMPARE(fe::crumbHitAt(layout, 60, 16, 3).slot, 1);
        QCOMPARE(fe::crumbHitAt(layout, 60, 16, 3).part, fe::CrumbPart::Label);
        // Nothing follows the folder on screen.
        QCOMPARE(fe::crumbHitAt(layout, 190, 16, 3).part, fe::CrumbPart::None);
        QCOMPARE(fe::crumbHitAt(layout, 190, 16, 3).slot, -1);
        QCOMPARE(fe::crumbHitAt(layout, -10, 16, 3).part, fe::CrumbPart::None);
    }

    void theEllipsisIsASlotOfItsOwn()
    {
        const fe::CrumbLayout layout = fe::layoutCrumbs({40, 50, 60, 70, 80}, 0, 10, 12, 250);
        const fe::CrumbHit hit = fe::crumbHitAt(layout, 55, 10, 3);
        QCOMPARE(hit.slot, 1);
        QCOMPARE(hit.part, fe::CrumbPart::Label);
        QCOMPARE(layout.cells.at(hit.slot).crumb, -1);
    }

    void filesDroppedOnAFolderOfThePathMoveOrCopyThere()
    {
        QCOMPARE(fe::crumbDrop(local("/home/alex"), true, {local("/home/alex/prueba/main.c")}, TAGS), fe::CrumbDrop::Transfer);
        QCOMPARE(fe::crumbDrop(local("/"), true, {local("/home/alex/a"), local("/home/alex/b")}, TAGS), fe::CrumbDrop::Transfer);
        QCOMPARE(fe::crumbDrop(QUrl(QStringLiteral("sftp://nas/share")), true, {local("/home/alex/a")}, TAGS),
                 fe::CrumbDrop::Transfer);
        QCOMPARE(fe::crumbDrop(QUrl(QStringLiteral("trash:/")), true, {local("/home/alex/a")}, TAGS), fe::CrumbDrop::Transfer);
    }

    void aFolderCannotGoIntoItselfOrItsOwnSubfolder()
    {
        QCOMPARE(fe::crumbDrop(local("/home/alex/prueba"), true, {local("/home/alex/prueba")}, TAGS), fe::CrumbDrop::Refused);
        QCOMPARE(fe::crumbDrop(local("/home/alex/prueba/sub"), true, {local("/home/alex/prueba/")}, TAGS), fe::CrumbDrop::Refused);
        QCOMPARE(fe::crumbDrop(local("/home/alex/prueba/sub"), true, {local("/home/alex/x"), local("/home/alex/prueba")}, TAGS),
                 fe::CrumbDrop::Refused);
    }

    void filesAlreadyInTheFolderStayWhereTheyAre()
    {
        QCOMPARE(fe::crumbDrop(local("/home/alex/prueba/"), true, {local("/home/alex/prueba/main.c"), local("/home/alex/prueba/hola")},
                               TAGS),
                 fe::CrumbDrop::Refused);
        // One file from elsewhere is reason enough: KIO asks about the others.
        QCOMPARE(fe::crumbDrop(local("/home/alex/prueba"), true, {local("/home/alex/prueba/main.c"), local("/tmp/x")}, TAGS),
                 fe::CrumbDrop::Transfer);
    }

    void onlyRealFoldersTakeFiles()
    {
        QCOMPARE(fe::crumbDrop(local("/home/alex/prueba/main.c"), false, {local("/tmp/x")}, TAGS), fe::CrumbDrop::Refused);
        QCOMPARE(fe::crumbDrop(QUrl(QStringLiteral("recentlyused:/files")), true, {local("/tmp/x")}, TAGS), fe::CrumbDrop::Refused);
        QCOMPARE(fe::crumbDrop(fe::filenameSearchUrl(QStringLiteral("mods"), GAMES, QStringLiteral("Search")), true,
                               {local("/tmp/x")}, TAGS),
                 fe::CrumbDrop::Refused);
        QCOMPARE(fe::crumbDrop(QUrl(), true, {local("/tmp/x")}, TAGS), fe::CrumbDrop::Refused);
        QCOMPARE(fe::crumbDrop(local("/home/alex"), true, {}, TAGS), fe::CrumbDrop::Refused);
    }

    void filesDroppedOnATagsShelfAreTaggedNotMoved()
    {
        QCOMPARE(fe::crumbDrop(QUrl::fromLocalFile(TAGS + QStringLiteral("/Red")), true, {local("/home/alex/a.txt")}, TAGS),
                 fe::CrumbDrop::Tag);
        // The shelf of all tags is no tag, and only files on this machine carry tags.
        QCOMPARE(fe::crumbDrop(QUrl::fromLocalFile(TAGS), true, {local("/home/alex/a.txt")}, TAGS), fe::CrumbDrop::Refused);
        QCOMPARE(fe::crumbDrop(QUrl::fromLocalFile(TAGS + QStringLiteral("/Red")), true, {QUrl(QStringLiteral("sftp://nas/a"))}, TAGS),
                 fe::CrumbDrop::Refused);
        // A folder opened from a shelf is a real folder.
        QCOMPARE(fe::crumbDrop(QUrl::fromLocalFile(TAGS + QStringLiteral("/Red/Mods")), true, {local("/home/alex/a.txt")}, TAGS),
                 fe::CrumbDrop::Transfer);
        // Without shelves the folder is just a folder.
        QCOMPARE(fe::crumbDrop(QUrl::fromLocalFile(TAGS + QStringLiteral("/Red")), true, {local("/home/alex/a.txt")}, QString()),
                 fe::CrumbDrop::Transfer);
    }
};

QTEST_GUILESS_MAIN(FinderTest)
#include "test_finder.moc"
