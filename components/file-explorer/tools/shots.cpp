// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// fe-shots: drives a File Explorer window through the states worth checking by eye and captures
// each one with Spectacle. It runs inside the test sandbox session and is not installed.
//
//   fe-shots OUTPUT_DIR FOLDER [states|menus|controls|drives|finder]
//
// "finder" needs FOLDER two levels below a folder it adds to the sidebar (X/2-Dev/<project>), holding get.sh,
// README.md, CHANGELOG.md and a scripts folder: the scene of the File Explorer mockup.
// FOLDER must hold "nnsd0zj.jpeg", a "Wallpapers" folder and a file whose name contains "ready".
// "menus" needs QT_QPA_PLATFORM=xcb: a Wayland compositor dismisses a popup that no real input
// event opened, so menus can only be captured from an X11 client.

#include "addressfield.h"
#include "application.h"
#include "browsertab.h"
#include "gitactions.h"
#include "mainwindow.h"
#include "tabbar.h"
#include "toolbar.h"
#include "sidebar.h"
#include "tagging.h"
#include "windowcontrols.h"

#include <KParts/ReadOnlyPart>

#include <QAction>
#include <QApplication>
#include <QContextMenuEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QToolTip>
#include <QProcess>
#include <QTimer>

#include <sys/stat.h>

#include <algorithm>
#include <functional>

namespace {

constexpr QSize WINDOW_SIZE{1280, 800};

struct Step {
    QString shot;  ///< The capture taken after the step; empty for none.
    int settleMs;  ///< How long the window gets to load and render before the capture.
    std::function<void()> action;
};

// Takes a full-screen capture with Spectacle to directory/name.png.
void capture(const QString &directory, const QString &name)
{
    const QString path = QDir(directory).filePath(name + QStringLiteral(".png"));
    const int code = QProcess::execute(QStringLiteral("spectacle"),
                                       {QStringLiteral("-b"), QStringLiteral("-n"), QStringLiteral("-f"), QStringLiteral("-o"), path});
    qInfo("captured %s (spectacle exit %d)", qPrintable(name), code);
}

/// Runs the steps one after another on the event loop, then quits.
class Script : public QObject
{
public:
    Script(QString directory, QList<Step> steps)
        : m_directory(std::move(directory))
        , m_steps(std::move(steps))
    {
    }

    void start()
    {
        QTimer::singleShot(0, this, [this] { next(); });
    }

private:
    void next()
    {
        if (m_index >= m_steps.size()) {
            QApplication::quit();
            return;
        }
        const Step &step = m_steps.at(m_index++);
        step.action();
        QTimer::singleShot(step.settleMs, this, [this, shot = step.shot] {
            if (!shot.isEmpty()) {
                capture(m_directory, shot);
            }
            next();
        });
    }

    QString m_directory;
    QList<Step> m_steps;
    qsizetype m_index = 0;
};

// The tab on screen; a window also keeps its other tabs as hidden children.
fe::BrowserTab *visibleTab(const MainWindow *window)
{
    const auto tabs = window->findChildren<fe::BrowserTab *>();
    for (fe::BrowserTab *tab : tabs) {
        if (tab->isVisible()) {
            return tab;
        }
    }
    return nullptr;
}

// The global position at fractions x and y (0 to 1) of the tab's file view.
QPoint viewPoint(fe::BrowserTab *tab, qreal x, qreal y)
{
    const auto *view = tab->findChild<QGraphicsView *>();
    return view->mapToGlobal(QPoint(qRound(view->width() * x), qRound(view->height() * y)));
}

/// Opens a context menu the way Dolphin asks for one, captures it from inside the menu's own event
/// loop, then closes it.
void captureContextMenu(const QString &directory, const QString &name, fe::BrowserTab *tab, const QPoint &pos,
                        const KFileItemList &items, const KParts::NavigationExtension::ActionGroupMap &groups)
{
    QTimer::singleShot(1500, [directory, name] {
        capture(directory, name);
        if (QWidget *popup = QApplication::activePopupWidget()) {
            popup->close();
        }
    });
    Q_EMIT tab->contextMenuRequested(pos, items, groups);
}

/// Opens a sidebar row's context menu through the event a right click sends, captures it, then closes it.
void captureSidebarMenu(const QString &directory, const QString &name, fe::SidebarView *view, const QModelIndex &index)
{
    if (!index.isValid()) {
        qWarning("no sidebar row for %s", qPrintable(name));
        return;
    }
    QTimer::singleShot(1500, [directory, name] {
        capture(directory, name);
        if (QWidget *popup = QApplication::activePopupWidget()) {
            popup->close();
        }
    });
    const QPoint local = view->visualRect(index).center();
    QContextMenuEvent event(QContextMenuEvent::Mouse, local, view->viewport()->mapToGlobal(local));
    QCoreApplication::sendEvent(view->viewport(), &event);
}

/// The first sidebar row of a mounted drive other than the system disk.
QModelIndex driveRow(const fe::SidebarView *view)
{
    const QAbstractItemModel *model = view->model();
    for (const char *prefix : {"/run/media/", "/media/", "/mnt/"}) {
        for (int row = 0; row < model->rowCount(); ++row) {
            const QModelIndex index = model->index(row, 0);
            const QUrl url = index.data(fe::SidebarModel::UrlRole).toUrl();
            if (url.isLocalFile() && url.toLocalFile().startsWith(QString::fromLatin1(prefix))) {
                return index;
            }
        }
    }
    return {};
}

/// Sends the events a pointer dragging urls sends over a sidebar row: enter and move at fraction of
/// the row's height (0 its top, 1 its bottom), and the drop when drop is set.
void dragOverSidebar(fe::SidebarView *view, const QModelIndex &index, qreal fraction, const QList<QUrl> &urls, bool drop)
{
    if (!index.isValid()) {
        qWarning("no sidebar row to drag over");
        return;
    }
    QMimeData data;
    data.setUrls(urls);
    const QRect row = view->visualRect(index);
    const QPointF pos(row.center().x(), row.top() + row.height() * fraction);
    const Qt::DropActions actions = Qt::CopyAction | Qt::MoveAction | Qt::LinkAction;
    QDragEnterEvent enter(pos, actions, &data, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(view->viewport(), &enter);
    QDragMoveEvent move(pos, actions, &data, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(view->viewport(), &move);
    if (drop) {
        QDropEvent dropped(pos, actions, &data, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(view->viewport(), &dropped);
    }
}

// Ends a drag over the sidebar.
void leaveSidebar(fe::SidebarView *view)
{
    QDragLeaveEvent leave;
    QCoreApplication::sendEvent(view->viewport(), &leave);
}

// Triggers the window action with this text, and logs whether it was enabled.
void triggerWindowAction(const MainWindow *window, const QString &text)
{
    const auto actions = window->actions();
    for (QAction *action : actions) {
        if (action->text() == text) {
            qInfo("%s: %s", qPrintable(text), action->isEnabled() ? "triggered" : "disabled");
            action->trigger();
            return;
        }
    }
    qWarning("no window action %s", qPrintable(text));
}

/// Puts the pointer over one of the window buttons (0 minimize, 1 maximize, 2 close) through the
/// same mouse-move event a real pointer sends.
void hoverWindowButton(const MainWindow *window, int segment)
{
    auto *controls = window->findChild<WindowControls *>();
    const qreal segmentWidth = controls->width() / 3.0;
    const QPointF local(segmentWidth * (segment + 0.5), controls->height() / 2.0);
    QMouseEvent move(QEvent::MouseMove, local, controls->mapToGlobal(local), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(controls, &move);
}

/// The sidebar row of the disk mounted at /.
QModelIndex systemDriveRow(const fe::SidebarView *view)
{
    const QAbstractItemModel *model = view->model();
    for (int row = 0; row < model->rowCount(); ++row) {
        const QModelIndex index = model->index(row, 0);
        if (index.data(fe::SidebarModel::IsDriveRole).toBool() && index.data(fe::SidebarModel::UrlRole).toUrl() == QUrl::fromLocalFile(QStringLiteral("/"))) {
            return index;
        }
    }
    return {};
}

/// Moves the pointer over a sidebar row through the event a real pointer sends, and shows the row's tooltip.
void hoverSidebarRow(fe::SidebarView *view, const QModelIndex &index, bool tooltip)
{
    const QPointF local = view->visualRect(index).center();
    QMouseEvent move(QEvent::MouseMove, local, view->viewport()->mapToGlobal(local), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(view->viewport(), &move);
    if (tooltip) {
        QToolTip::showText(view->viewport()->mapToGlobal(local.toPoint()) + QPoint(8, 14), index.data(Qt::ToolTipRole).toString(), view->viewport());
    }
}

/// Types text into the sidebar's rename field and presses Return, as the keyboard would.
void typeRename(fe::SidebarView *view, const QString &text)
{
    auto *editor = view->viewport()->findChild<QLineEdit *>();
    if (!editor) {
        qWarning("no rename field");
        return;
    }
    editor->setText(text);
    QKeyEvent press(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier);
    QCoreApplication::sendEvent(editor, &press);
}

// Triggers the window's Hide Sidebar or Show Sidebar action, whichever it offers now.
void toggleSidebar(const MainWindow *window)
{
    const auto actions = window->findChildren<QAction *>();
    for (QAction *action : actions) {
        if (action->text() == u"Hide Sidebar" || action->text() == u"Show Sidebar") {
            action->trigger();
            return;
        }
    }
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    fe::configureApplication();
    const QStringList arguments = QApplication::arguments();
    if (arguments.size() < 3 || arguments.size() > 4) {
        qCritical("usage: fe-shots OUTPUT_DIR FOLDER [states|menus|controls|drives|finder|git]");
        return 2;
    }
    const QString mode = arguments.size() == 4 ? arguments.at(3) : QStringLiteral("states");
    const QString out = arguments.at(1);
    const QString folder = QDir::cleanPath(arguments.at(2));
    const QUrl wallpapers = QUrl::fromLocalFile(folder + QStringLiteral("/Wallpapers"));

    auto *window = new MainWindow(QUrl::fromLocalFile(folder), fe::currentPathContext());
    window->resize(WINDOW_SIZE);
    window->show();

    const auto tab = [window] { return visibleTab(window); };
    const auto sidebar = [window] { return window->findChild<fe::SidebarView *>(); };
    const auto sidebarModel = [sidebar] { return qobject_cast<fe::SidebarModel *>(sidebar()->model()); };
    const auto part = [tab] { return tab()->findChild<KParts::ReadOnlyPart *>(); };
    const auto editGroups = [tab] {
        // The group DolphinPart::slotOpenContextMenu sends for items (dolphinpart.cpp, v26.08.2, line 422).
        QList<QAction *> edit;
        for (const char *name : {"movetotrash", "renamefile"}) {
            if (QAction *action = tab()->partAction(QString::fromLatin1(name))) {
                edit.append(action);
            }
        }
        return KParts::NavigationExtension::ActionGroupMap{{QStringLiteral("editactions"), edit}};
    };

    const auto selectPhoto = [tab, part, folder] {
        tab()->goBack();
        // Dolphin forgets the files to select when it changes folder (dolphinview.cpp:849), so
        // they are set after the navigation starts and applied when the listing completes.
        const QList<QUrl> files{QUrl::fromLocalFile(folder + QStringLiteral("/nnsd0zj.jpeg"))};
        part()->setProperty("filesToSelect", QVariant::fromValue(files));
        tab()->focusView();
    };
    const QList<Step> menuSteps{
        {QString(), 4000, [] {}},
        {QString(), 1500, [tab, wallpapers] { tab()->openFolder(wallpapers); }},
        {QString(), 2500, selectPhoto},
        {QString(), 300, [out, tab, editGroups] {
             captureContextMenu(out, QStringLiteral("fe-menu-item"), tab(), viewPoint(tab(), 0.40, 0.33), tab()->selectedItems(), editGroups());
         }},
        {QString(), 2500, [tab] { tab()->setViewMode(fe::ViewMode::List); }},
        {QString(), 300, [out, tab] {
             const KFileItem folderItem(tab()->url(), QStringLiteral("inode/directory"), S_IFDIR);
             captureContextMenu(out, QStringLiteral("fe-menu-folder"), tab(), viewPoint(tab(), 0.55, 0.62), KFileItemList{folderItem}, {});
         }},
        // The Group menu runs its own event loop (QMenu::exec), so it opens from a timer: the step returns at once and
        // the capture and the next step, which closes it, still run.
        {QStringLiteral("fe-menu-group"), 1500, [window] {
             QTimer::singleShot(0, window, [window] { Q_EMIT window->findChild<fe::Toolbar *>()->groupClicked(); });
         }},
        {QString(), 300, [] {
             if (QWidget *popup = QApplication::activePopupWidget()) {
                 popup->close();
             }
         }},
        // Add to Sidebar, end to end: the folder's menu offers it, the File menu adds it, its row's menu removes it.
        {QString(), 2500, [tab, wallpapers] { tab()->openFolder(wallpapers); }},
        {QString(), 300, [out, tab] {
             const KFileItem folderItem(tab()->url(), QStringLiteral("inode/directory"), S_IFDIR);
             captureContextMenu(out, QStringLiteral("fe-menu-folder-add"), tab(), viewPoint(tab(), 0.55, 0.62), KFileItemList{folderItem}, {});
         }},
        {QStringLiteral("fe-sidebar-added"), 2000, [window] { triggerWindowAction(window, QStringLiteral("Add to Sidebar")); }},
        {QString(), 300, [out, sidebar, sidebarModel, wallpapers] {
             captureSidebarMenu(out, QStringLiteral("fe-menu-sidebar-folder"), sidebar(), sidebarModel()->indexForUrl(wallpapers));
         }},
        {QString(), 300, [out, sidebar] { captureSidebarMenu(out, QStringLiteral("fe-menu-sidebar-drive"), sidebar(), driveRow(sidebar())); }},
        {QString(), 300, [out, sidebar, sidebarModel] {
             captureSidebarMenu(out, QStringLiteral("fe-menu-sidebar-trash"), sidebar(), sidebarModel()->indexForUrl(QUrl(QStringLiteral("trash:/"))));
         }},
        {QStringLiteral("fe-sidebar-removed"), 2000, [sidebarModel, wallpapers] { sidebarModel()->removeFromSidebar(sidebarModel()->indexForUrl(wallpapers)); }},
        // Drag and drop: a folder held at the bottom edge of Videos shows where it joins, the drop adds it
        // there, and a file held over a folder lights the folder up.
        {QStringLiteral("fe-sidebar-drag-insert"), 1200, [sidebar, sidebarModel, folder] {
             const QUrl videos = QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
             dragOverSidebar(sidebar(), sidebarModel()->indexForUrl(videos), 0.92, {QUrl::fromLocalFile(folder + QStringLiteral("/Mods"))}, false);
         }},
        {QStringLiteral("fe-sidebar-dropped"), 2000, [sidebar, sidebarModel, folder] {
             const QUrl videos = QUrl::fromLocalFile(QStandardPaths::writableLocation(QStandardPaths::MoviesLocation));
             dragOverSidebar(sidebar(), sidebarModel()->indexForUrl(videos), 0.92, {QUrl::fromLocalFile(folder + QStringLiteral("/Mods"))}, true);
         }},
        {QStringLiteral("fe-sidebar-drag-into"), 1200, [sidebar, sidebarModel, folder] {
             dragOverSidebar(sidebar(), sidebarModel()->indexForUrl(QUrl::fromLocalFile(folder + QStringLiteral("/Mods"))), 0.5,
                             {QUrl::fromLocalFile(folder + QStringLiteral("/nnsd0zj.jpeg"))}, false);
         }},
        {QString(), 1500, [sidebar, sidebarModel, folder] {
             leaveSidebar(sidebar());
             sidebarModel()->removeFromSidebar(sidebarModel()->indexForUrl(QUrl::fromLocalFile(folder + QStringLiteral("/Mods"))));
         }},
    };

    const QList<Step> stateSteps{
        {QStringLiteral("fe-1-icons"), 4000, [] {}},
        {QString(), 1500, [tab, wallpapers] { tab()->openFolder(wallpapers); }},
        {QStringLiteral("fe-2-selected"), 2500, selectPhoto},
        {QStringLiteral("fe-4-list"), 2500, [tab] { tab()->setViewMode(fe::ViewMode::List); }},
        {QStringLiteral("fe-6-tabs"), 4000, [window, wallpapers] { window->openTab(wallpapers); }},
        {QStringLiteral("fe-7-search"), 3500, [window] {
             Q_EMIT window->findChild<fe::PillTabBar *>()->tabClicked(0);
             window->findChild<fe::Toolbar *>()->searchField()->setText(QStringLiteral("ready"));
         }},
        {QStringLiteral("fe-8-no-sidebar"), 2000, [window] {
             window->findChild<fe::Toolbar *>()->searchField()->clear();
             toggleSidebar(window);
         }},
        {QString(), 500, [window] { toggleSidebar(window); }},
    };
    const QList<Step> controlSteps{
        {QStringLiteral("ctl-1-normal"), 4500, [] {}},
        {QStringLiteral("ctl-2-hover-close"), 700, [window] { hoverWindowButton(window, 2); }},
        {QStringLiteral("ctl-3-hover-maximize"), 700, [window] { hoverWindowButton(window, 1); }},
        {QStringLiteral("ctl-4-hover-minimize"), 700, [window] { hoverWindowButton(window, 0); }},
    };
    // The drives of the sidebar: names, symbols and capacity bars; a hover thickens the bar and shows free of total;
    // Rename takes a name inline and the path bar follows; clearing the name gives the default back.
    const QList<Step> driveSteps{
        {QStringLiteral("drv-1-sidebar"), 4500, [window, sidebar] {
             qInfo("window at %d %d %d %d", window->x(), window->y(), window->width(), window->height());
             QTimer::singleShot(4000, [sidebar] { qInfo("before: capacity %s", qPrintable(systemDriveRow(sidebar()).data(fe::SidebarModel::CapacityRole).toString())); });
         }},
        {QStringLiteral("drv-2-hover"), 600, [sidebar] { hoverSidebarRow(sidebar(), systemDriveRow(sidebar()), false); }},
        {QStringLiteral("drv-3-tooltip"), 900, [sidebar] { hoverSidebarRow(sidebar(), systemDriveRow(sidebar()), true); }},
        {QString(), 300, [] { QToolTip::hideText(); }},
        {QStringLiteral("drv-4-menu"), 300, [out, sidebar] { captureSidebarMenu(out, QStringLiteral("drv-4-menu"), sidebar(), systemDriveRow(sidebar())); }},
        {QStringLiteral("drv-5-rename"), 2200, [sidebar] { sidebar()->edit(systemDriveRow(sidebar())); }},
        {QStringLiteral("drv-6-renamed"), 1200, [sidebar] { typeRename(sidebar(), QStringLiteral("  Mi   Torre  ")); }},
        {QString(), 300, [sidebar] {
             const QModelIndex row = systemDriveRow(sidebar());
             qInfo("renamed: %s capacity %s", qPrintable(row.data(Qt::DisplayRole).toString()), qPrintable(row.data(fe::SidebarModel::CapacityRole).toString()));
             sidebar()->edit(systemDriveRow(sidebar()));
         }},
        {QStringLiteral("drv-7-default"), 1200, [sidebar] { typeRename(sidebar(), QString()); }},
        {QString(), 200, [sidebar] { qInfo("default again: %s", qPrintable(systemDriveRow(sidebar()).data(Qt::DisplayRole).toString())); }},
    };
    // The mockup's scene: the project's grandparent in Favorites, unfolded down to the project, two tagged files, get.sh
    // selected; then its menu, a typed path with suggestions, Quick Look, Split View and a tag's shelf.
    const QString grandparent = QFileInfo(QFileInfo(folder).absolutePath()).absolutePath();
    const auto file = [folder](const char *name) { return QUrl::fromLocalFile(folder + u'/' + QLatin1String(name)); };
    const auto select = [tab](const QList<QUrl> &files) {
        tab()->selectInPlace(files);
        tab()->focusView();
    };
    const auto typeInto = [](QWidget *widget, const QString &text) {
        for (const QChar ch : text) {
            QKeyEvent press(QEvent::KeyPress, 0, Qt::NoModifier, QString(ch));
            QCoreApplication::sendEvent(widget, &press);
        }
    };
    const auto windowAction = [window](const QString &text) {
        for (QAction *action : window->findChildren<QAction *>()) {
            if (action->text() == text) {
                action->trigger();
                return;
            }
        }
        qWarning("no action %s", qPrintable(text));
    };
    const QList<Step> finderSteps{
        {QString(), 3000, [sidebar, sidebarModel, grandparent, file] {
             fe::tagging::setTag({file("CHANGELOG.md")}, QStringLiteral("Green"), true);
             fe::tagging::setTag({file("README.md")}, QStringLiteral("Blue"), true);
             // The sandbox keeps its places between runs: the folder joins once and unfolds only when folded.
             if (!sidebarModel()->indexForUrl(QUrl::fromLocalFile(grandparent)).isValid()) {
                 sidebar()->addFolder(QUrl::fromLocalFile(grandparent), QFileInfo(grandparent).fileName());
             }
             const auto unfold = [sidebarModel](const QString &path) {
                 const QModelIndex row = sidebarModel()->indexForUrl(QUrl::fromLocalFile(path));
                 if (!row.data(fe::SidebarModel::ExpandedRole).toBool()) {
                     sidebarModel()->toggleExpanded(row);
                 }
             };
             QTimer::singleShot(1200, [unfold, grandparent] {
                 unfold(grandparent);
                 unfold(grandparent + u'/' + QDir(grandparent).entryList(QDir::Dirs | QDir::NoDotAndDotDot).constFirst());
             });
         }},
        {QStringLiteral("fe-f1-main"), 3000, [tab, select, file] {
             tab()->setViewMode(fe::ViewMode::List);
             select({file("get.sh")});
         }},
        {QString(), 300, [out, tab] {
             captureContextMenu(out, QStringLiteral("fe-f2-menu"), tab(), viewPoint(tab(), 0.42, 0.40), tab()->selectedItems(), {});
         }},
        {QStringLiteral("fe-f3-address"), 1500, [window, typeInto, folder] {
             fe::AddressField *field = window->findChild<fe::AddressField *>();
             field->startEditing();
             // After the field's own select-all, as a person types after clicking.
             QTimer::singleShot(300, [field, typeInto, folder] {
                 field->clear();
                 typeInto(field, folder + QStringLiteral("/s"));
             });
         }},
        {QStringLiteral("fe-f4-quicklook"), 2500, [window, select, file, windowAction] {
             window->findChild<fe::AddressField *>()->editingCancelled();
             select({file("README.md")});
             QTimer::singleShot(1200, [windowAction] { windowAction(QStringLiteral("Quick Look")); });
         }},
        {QStringLiteral("fe-f5-split"), 2500, [window, windowAction] {
             for (QWidget *top : QApplication::topLevelWidgets()) {
                 if (top != window && top->isVisible() && top->windowTitle() == u"Quick Look") {
                     top->close();
                 }
             }
             windowAction(QStringLiteral("Split View"));
         }},
        {QStringLiteral("fe-f6-tag"), 2500, [sidebarModel, windowAction] {
             windowAction(QStringLiteral("Split View"));
             sidebarModel()->activate(sidebarModel()->indexForUrl(QUrl::fromLocalFile(fe::tagging::shelfRoot() + QStringLiteral("/Green"))));
         }},
        // Group: the toolbar's menu, then Kind chosen from it, as a person would.
        {QStringLiteral("fe-f7-group-menu"), 2500, [tab, window, folder] {
             tab()->openFolder(QUrl::fromLocalFile(folder));
             QTimer::singleShot(1200, [window] { Q_EMIT window->findChild<fe::Toolbar *>()->groupClicked(); });
         }},
        {QStringLiteral("fe-f8-grouped"), 2000, [] {
             auto *menu = qobject_cast<QMenu *>(QApplication::activePopupWidget());
             if (!menu) {
                 qWarning("FAIL: Group opened no menu");
                 return;
             }
             const QList<QAction *> actions = menu->actions();
             QStringList entries;
             for (const QAction *action : actions) {
                 entries.append(action->text() + (action->isChecked() ? QStringLiteral(" (checked)") : QString()));
             }
             qInfo("Group menu: %s", qPrintable(entries.join(QStringLiteral(", "))));
             const auto kind = std::find_if(actions.cbegin(), actions.cend(), [](const QAction *action) { return action->text() == u"Kind"; });
             if (kind != actions.cend()) {
                 (*kind)->trigger();
             } else {
                 qWarning("FAIL: the Group menu has no Kind");
             }
             menu->close();
         }},
        // Another window takes the focus, as when the pointer works on another monitor, and this one turns inactive.
        {QStringLiteral("fe-f9-inactive"), 2500, [] {
             auto *elsewhere = new QWidget(nullptr, Qt::Window);
             elsewhere->setWindowTitle(QStringLiteral("Elsewhere"));
             elsewhere->setGeometry(60, 120, 360, 220);
             elsewhere->show();
             elsewhere->activateWindow();
         }},
    };
    // Git's sheets over a repository: folder is the repository, and its parent the folder a clone goes into.
    const auto gitContext = [window, folder] {
        fe::GitMenuContext context;
        context.target.kind = fe::git::Target::Kind::Repository;
        context.target.repository = fe::git::repositoryOf(folder, true).value_or(fe::git::Repository{});
        context.target.folder = QFileInfo(folder).absolutePath();
        context.window = window;
        return context;
    };
    const auto openGit = [gitContext](fe::git::Command command) {
        const fe::GitMenuContext context = gitContext();
        fe::loadGitSnapshot(context.target.repository, {}, context.window, [context, command](const std::optional<fe::GitSnapshot> &snapshot) {
            if (snapshot) {
                fe::runGitCommand(command, context, *snapshot);
            }
        });
    };
    const auto closeSheet = [] {
        if (QWidget *sheet = QApplication::activeModalWidget()) {
            sheet->close();
        }
    };
    const QList<Step> gitSteps{
        {QStringLiteral("fe-git-window"), 4000, [] {}},
        {QStringLiteral("fe-git-commit"), 2500, [openGit] { openGit(fe::git::Command::Commit); }},
        {QString(), 400, closeSheet},
        {QStringLiteral("fe-git-changes"), 2500, [openGit] { openGit(fe::git::Command::Changes); }},
        {QString(), 400, closeSheet},
        {QStringLiteral("fe-git-history"), 2500, [openGit] { openGit(fe::git::Command::History); }},
        {QString(), 400, closeSheet},
        {QStringLiteral("fe-git-branches"), 2500, [openGit] { openGit(fe::git::Command::Branches); }},
        {QString(), 400, closeSheet},
        {QStringLiteral("fe-git-clone"), 1500, [gitContext] {
             fe::GitMenuContext context = gitContext();
             context.target.kind = fe::git::Target::Kind::Outside;
             fe::runGitCommand(fe::git::Command::Clone, context, {});
         }},
        {QString(), 400, closeSheet},
    };

    const QList<Step> &steps = mode == u"menus"      ? menuSteps
        : mode == u"controls"                         ? controlSteps
        : mode == u"drives"                           ? driveSteps
        : mode == u"finder"                           ? finderSteps
        : mode == u"git"                              ? gitSteps
                                                      : stateSteps;
    Script script(out, steps);
    script.start();
    return QApplication::exec();
}
