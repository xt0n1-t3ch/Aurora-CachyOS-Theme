// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements filemanager1.h: each call opens main windows, optionally with items selected.

#include "filemanager1.h"

#include "core/reveal.h"
#include "mainwindow.h"

#include <KPropertiesDialog>
#include <KWindowSystem>

#include <QDBusConnection>

FileManager1::FileManager1(fe::PathContext context, QObject *parent)
    : QObject(parent)
    , m_context(std::move(context))
{
}

bool FileManager1::registerOnSessionBus()
{
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.registerObject(QStringLiteral("/org/freedesktop/FileManager1"), this, QDBusConnection::ExportScriptableSlots)) {
        return false;
    }
    return bus.registerService(QStringLiteral("org.freedesktop.FileManager1"));
}

void FileManager1::ShowFolders(const QStringList &uriList, const QString &startUpId)
{
    for (const QUrl &folder : fe::foldersToShow(uriList)) {
        openWindow(folder, {}, startUpId);
    }
}

void FileManager1::ShowItems(const QStringList &uriList, const QString &startUpId)
{
    for (const fe::Reveal &group : fe::revealGroups(uriList)) {
        openWindow(group.folder, group.items, startUpId);
    }
}

void FileManager1::ShowItemProperties(const QStringList &uriList, const QString &startUpId)
{
    const QList<fe::Reveal> groups = fe::revealGroups(uriList);
    for (const fe::Reveal &group : groups) {
        openWindow(group.folder, group.items, startUpId);
    }
    QList<QUrl> items;
    for (const fe::Reveal &group : groups) {
        items += group.items;
    }
    if (!items.isEmpty()) {
        KPropertiesDialog::showDialog(items);
    }
}

void FileManager1::openWindow(const QUrl &folder, const QList<QUrl> &select, const QString &startUpId)
{
    auto *window = new MainWindow(folder, m_context);
    window->setAttribute(Qt::WA_DeleteOnClose);
    if (!select.isEmpty()) {
        window->selectItems(select);
    }
    // The caller's activation token lets the new window come to the front on Wayland.
    if (!startUpId.isEmpty()) {
        KWindowSystem::setCurrentXdgActivationToken(startUpId);
    }
    window->show();
    window->raise();
    window->activateWindow();
    Q_EMIT windowOpened();
}
