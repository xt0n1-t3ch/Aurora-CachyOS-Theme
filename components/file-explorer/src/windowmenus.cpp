// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The menus that drop down from the toolbar's buttons: Group, Share, Edit Tags and More.

#include "mainwindow.h"

#include "browsertab.h"
#include "sidebar.h"
#include "tagging.h"
#include "toolbar.h"

#include <Purpose/AlternativesModel>
#include <Purpose/Menu>

#include <QDir>
#include <QJsonArray>
#include <QJsonObject>
#include <QMenu>
#include <QMimeDatabase>

namespace {

/// Opens menu right below button, aligned to its left edge, and waits for the choice.
void execBelow(QMenu &menu, const QWidget *button)
{
    menu.exec(button->mapToGlobal(QPoint(0, button->height() + 6)));
}

/// The MIME type that covers every item: their own when they share it, else the common family, else any file.
QString commonMimeType(const KFileItemList &items)
{
    QString common;
    for (const KFileItem &item : items) {
        const QString type = item.mimetype();
        if (common.isEmpty()) {
            common = type;
        } else if (common != type) {
            const QString family = type.section(u'/', 0, 0);
            common = common.startsWith(family + u'/') ? family + QStringLiteral("/*") : QStringLiteral("application/octet-stream");
        }
    }
    return common;
}

} // namespace

void MainWindow::showGroupMenu()
{
    fe::BrowserTab *tab = currentTab();
    QAction *groups = tab->partAction(QStringLiteral("show_in_groups"));
    if (!groups) {
        return;
    }
    // Dolphin groups by the column it sorts by, so a group is a sort choice with groups on.
    const struct {
        const char *label;
        const char *sortAction;
    } choices[] = {
        {"None", nullptr},
        {"Name", "sort_by_text"},
        {"Kind", "sort_by_type"},
        {"Date Modified", "sort_by_modificationtime"},
        {"Size", "sort_by_size"},
    };
    QMenu menu(this);
    menu.setToolTipsVisible(true);
    menu.addSection(QStringLiteral("Group By"));
    for (const auto &choice : choices) {
        QAction *sort = choice.sortAction ? tab->partAction(QString::fromLatin1(choice.sortAction)) : nullptr;
        if (choice.sortAction && !sort) {
            continue;
        }
        QAction *entry = menu.addAction(QString::fromLatin1(choice.label));
        entry->setCheckable(true);
        entry->setChecked(sort ? groups->isChecked() && sort->isChecked() : !groups->isChecked());
        connect(entry, &QAction::triggered, this, [groups, sort] {
            if (sort && !sort->isChecked()) {
                sort->trigger();
            }
            if (groups->isChecked() != (sort != nullptr)) {
                groups->trigger();
            }
        });
    }
    execBelow(menu, m_toolbar->groupButton());
}

void MainWindow::showShareMenu()
{
    const KFileItemList items = currentTab()->selectedItems();
    if (items.isEmpty()) {
        return;
    }
    QJsonArray urls;
    for (const KFileItem &item : items) {
        urls.append(item.targetUrl().toString());
    }
    Purpose::Menu menu(this);
    menu.model()->setInputData(QJsonObject{{QStringLiteral("urls"), urls}, {QStringLiteral("mimeType"), commonMimeType(items)}});
    menu.model()->setPluginType(QStringLiteral("Export"));
    menu.reload();
    if (menu.isEmpty()) {
        menu.addAction(QStringLiteral("Nothing on this computer can share these"))->setEnabled(false);
    }
    execBelow(menu, m_toolbar->shareButton());
}

void MainWindow::showTagsMenu()
{
    const QList<QUrl> items = selectedUrls();
    if (items.isEmpty()) {
        return;
    }
    QMenu menu(this);
    fe::tagging::addTagEntries(menu, items);
    execBelow(menu, m_toolbar->tagsButton());
}

void MainWindow::showMoreMenu()
{
    fe::BrowserTab *tab = currentTab();
    QMenu menu(this);
    const auto addPart = [&menu, tab](const char *name) {
        if (QAction *action = tab->partAction(QString::fromLatin1(name))) {
            menu.addAction(action);
        }
    };
    addPart("show_hidden_files");
    addPart("show_preview");
    addPart("sort");
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("document-properties")), QStringLiteral("Get Info\tCtrl+I"), this, &MainWindow::showInfo);
    menu.addAction(m_addToSidebar);
    addPart("copy_location");
    menu.addSeparator();
    menu.addAction(QIcon::fromTheme(QStringLiteral("window-new")), QStringLiteral("New Window\tCtrl+N"), this, &MainWindow::openNewWindow);
    menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")), QStringLiteral("New Tab\tCtrl+T"), this,
                   [this] { openTab(QUrl::fromLocalFile(QDir::homePath())); });
    menu.addAction(m_sidebarAction);
    syncEditActions();
    execBelow(menu, m_toolbar->moreButton());
}
