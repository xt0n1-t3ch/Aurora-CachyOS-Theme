// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Back and forward history for one browser tab.

#pragma once

#include <QList>
#include <QUrl>

/// The back and forward history of one browser tab, kept the way a web browser keeps it.
class NavigationHistory
{
public:
    /// Records a visit. Visiting the current folder again changes nothing; any other visit drops the
    /// forward entries.
    void visit(const QUrl &url);

    [[nodiscard]] bool canGoBack() const;
    [[nodiscard]] bool canGoForward() const;

    /// Steps back and returns the folder to show. At the first entry it stays where it is.
    QUrl back();
    /// Steps forward and returns the folder to show. At the last entry it stays where it is.
    QUrl forward();

    [[nodiscard]] QUrl current() const;

private:
    QList<QUrl> m_entries;
    qsizetype m_index = -1;
};
