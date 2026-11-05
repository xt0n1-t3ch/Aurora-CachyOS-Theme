// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements history.h.

#include "core/history.h"

void NavigationHistory::visit(const QUrl &url)
{
    if (m_index >= 0 && m_entries.at(m_index).matches(url, QUrl::StripTrailingSlash)) {
        return;
    }
    m_entries.resize(m_index + 1);
    m_entries.append(url);
    m_index = m_entries.size() - 1;
}

bool NavigationHistory::canGoBack() const
{
    return m_index > 0;
}

bool NavigationHistory::canGoForward() const
{
    return m_index >= 0 && m_index < m_entries.size() - 1;
}

QUrl NavigationHistory::back()
{
    if (canGoBack()) {
        --m_index;
    }
    return current();
}

QUrl NavigationHistory::forward()
{
    if (canGoForward()) {
        ++m_index;
    }
    return current();
}

QUrl NavigationHistory::current() const
{
    return m_index >= 0 ? m_entries.at(m_index) : QUrl();
}
