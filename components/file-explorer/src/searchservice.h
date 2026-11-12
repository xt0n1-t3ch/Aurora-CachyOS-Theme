// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// File Explorer's link to aurora-search, the whole-disk name search helper: one connection per process, opened
// when a window is in use and let go after a few idle minutes, so the helper runs only while File Explorer is used.

#pragma once

#include <QObject>
#include <QUrl>

class QSocketNotifier;
class QTimer;

namespace fe {

/// The connection to the helper's socket (`AURORA_SEARCH_SOCKET`, by default /run/aurora/search.sock), which
/// systemd answers by starting the helper. GUI thread only.
class SearchService : public QObject
{
    Q_OBJECT

public:
    /// The process's one service.
    static SearchService &instance();

    /// Connects now, if not yet connected, and keeps the connection for the idle time from now. Called when a
    /// window becomes active and when a search starts.
    void warm();
    /// Whether the helper answers and holds `folder` in its index (an empty URL: the whole PC). Waits at most a
    /// fraction of a second; false without the helper.
    [[nodiscard]] bool covers(const QUrl &folder);
    /// The socket path workers and this connection use.
    [[nodiscard]] static QString socketPath();

Q_SIGNALS:
    /// The helper's index changed: a search on screen should list again.
    void indexChanged();

private:
    SearchService();
    ~SearchService() override;
    void connectNow();
    void disconnectNow();
    void readMessages();

    int m_fd = -1;
    QSocketNotifier *m_notifier = nullptr;
    QTimer *m_letGo;
    quint64 m_nextId = 1;
};

} // namespace fe
