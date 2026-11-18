// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements history.h.

#include "core/history.h"

#include "core/paths.h"

void NavigationHistory::seed(const QList<QUrl> &chain)
{
    if (!m_entries.isEmpty()) {
        return;
    }
    for (const QUrl &url : chain) {
        if (!url.isValid()) {
            continue;
        }
        if (m_entries.isEmpty() || !m_entries.constLast().matches(url, QUrl::StripTrailingSlash)) {
            m_entries.append(url);
        }
    }
    m_index = m_entries.size() - 1;
}

void NavigationHistory::visit(const QUrl &url)
{
    if (m_index >= 0 && m_entries.at(m_index).matches(url, QUrl::StripTrailingSlash)) {
        return;
    }
    m_entries.resize(m_index + 1);
    m_entries.append(url);
    m_index = m_entries.size() - 1;
}

void NavigationHistory::stepUp(const QUrl &ancestor)
{
    if (m_index < 0) {
        visit(ancestor);
        return;
    }
    if (m_entries.at(m_index).matches(ancestor, QUrl::StripTrailingSlash)) {
        return;
    }
    qsizetype top = m_index;
    while (top > 0) {
        const QUrl &before = m_entries.at(top - 1);
        if (before.matches(ancestor, QUrl::StripTrailingSlash)) {
            m_index = top - 1;
            return;
        }
        if (!fe::isStrictAncestorUrl(before, m_entries.at(top)) || !fe::isStrictAncestorUrl(ancestor, before)) {
            break;
        }
        --top;
    }
    m_entries.insert(top, ancestor);
    m_index = top;
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
