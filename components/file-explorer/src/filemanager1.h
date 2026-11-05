// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The org.freedesktop.FileManager1 service, so other programs' "Show in folder" opens File Explorer.

#pragma once

#include "core/paths.h"

#include <QList>
#include <QObject>
#include <QStringList>
#include <QUrl>

/// org.freedesktop.FileManager1 on the session bus: what browsers, Steam and Plasma call for "Show in folder" and
/// "Open containing folder". Every call opens File Explorer windows, so Dolphin's old look never comes up.
class FileManager1 : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.FileManager1")

public:
    explicit FileManager1(fe::PathContext context, QObject *parent = nullptr);

    /// Takes the bus name and exports the object; false when another file manager already holds the name.
    bool registerOnSessionBus();

public Q_SLOTS:
    /// Opens a window for each folder in uriList. startUpId is the caller's activation token.
    Q_SCRIPTABLE void ShowFolders(const QStringList &uriList, const QString &startUpId);
    /// Opens a window for each parent folder of the items in uriList, with those items selected.
    Q_SCRIPTABLE void ShowItems(const QStringList &uriList, const QString &startUpId);
    /// Does what ShowItems does, then shows the Properties dialog for all the items.
    Q_SCRIPTABLE void ShowItemProperties(const QStringList &uriList, const QString &startUpId);

Q_SIGNALS:
    /// A window opened in answer to a call.
    void windowOpened();

private:
    /// Opens a window on folder with select selected, and raises it with the caller's activation token.
    void openWindow(const QUrl &folder, const QList<QUrl> &select, const QString &startUpId);

    fe::PathContext m_context;
};
