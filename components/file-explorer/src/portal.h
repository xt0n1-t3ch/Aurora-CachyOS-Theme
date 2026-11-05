// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The FileChooser portal backend and the request object that lets the caller cancel an open panel.

#pragma once

#include "core/chooser.h"
#include "core/paths.h"

#include <QDBusAbstractAdaptor>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVirtualObject>
#include <QVariantMap>

/// org.freedesktop.impl.portal.Request at a call's handle: xdg-desktop-portal calls Close() when the app gives up.
class PortalRequest : public QDBusVirtualObject
{
    Q_OBJECT

public:
    explicit PortalRequest(const QDBusObjectPath &handle, QObject *parent = nullptr);
    ~PortalRequest() override;

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override;
    [[nodiscard]] QString introspect(const QString &path) const override;

Q_SIGNALS:
    /// The portal called Close(): the app no longer wants the panel.
    void closeRequested();

private:
    QString m_path;
};

/// The FileChooser portal backend: every Open and Save panel an app asks xdg-desktop-portal for is a File
/// Explorer window with the panel's buttons, so the system's file dialogs share the Finder look.
/// Same wire types as xdg-desktop-portal-kde (filechooser.h): filters are a(sa(us)).
class FileChooserPortal : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.impl.portal.FileChooser")
    Q_PROPERTY(uint version READ version CONSTANT)

public:
    struct Filter {
        uint type = 0; // 0: glob, 1: MIME type
        QString filterString;
    };
    using Filters = QList<Filter>;
    struct FilterList {
        QString userVisibleName;
        Filters filters;
    };
    using FilterListList = QList<FilterList>;

    FileChooserPortal(fe::PathContext context, QObject *parent);

    [[nodiscard]] uint version() const;

public Q_SLOTS:
    /// The three portal methods show a panel and block in a local event loop until the user answers. replyResponse is
    /// 0 for an accepted panel and 1 for a cancelled one; replyResults holds the chosen "uris", "current_filter" and
    /// "writable". The app id and the message are not used.
    void OpenFile(const QDBusObjectPath &handle, const QString &app_id, const QString &parent_window, const QString &title,
                  const QVariantMap &options, const QDBusMessage &message, uint &replyResponse, QVariantMap &replyResults);
    void SaveFile(const QDBusObjectPath &handle, const QString &app_id, const QString &parent_window, const QString &title,
                  const QVariantMap &options, const QDBusMessage &message, uint &replyResponse, QVariantMap &replyResults);
    void SaveFiles(const QDBusObjectPath &handle, const QString &app_id, const QString &parent_window, const QString &title,
                   const QVariantMap &options, const QDBusMessage &message, uint &replyResponse, QVariantMap &replyResults);

private:
    enum class Mode { Open, Save, SaveFiles };
    /// Shows the panel for mode, waits for the answer and fills the reply of the three methods above.
    void run(Mode mode, const QDBusObjectPath &handle, const QString &parentWindow, const QString &title,
             const QVariantMap &options, uint &replyResponse, QVariantMap &replyResults);

    fe::PathContext m_context;
};

Q_DECLARE_METATYPE(FileChooserPortal::Filter)
Q_DECLARE_METATYPE(FileChooserPortal::FilterList)
