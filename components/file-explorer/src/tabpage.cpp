// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements tabpage.h.

#include "tabpage.h"

#include "browsertab.h"

#include <QApplication>

namespace fe {

TabPage::TabPage(BrowserTab *primary, QWidget *parent)
    : QSplitter(Qt::Horizontal, parent)
    , m_active(primary)
{
    setChildrenCollapsible(false);
    // A hairline between the panes, drawn by FinderStyle (CE_Splitter). A style sheet here would hand every widget of
    // both panes to Qt's style sheet style, which fills line edits with an opaque panel.
    setHandleWidth(1);
    addWidget(primary);
    // The pane that gets the keyboard, by a click or by Tab, becomes the active one.
    connect(qApp, &QApplication::focusChanged, this, [this](QWidget *, QWidget *now) {
        for (BrowserTab *pane : panes()) {
            if (now && pane->isAncestorOf(now)) {
                setActivePane(pane);
                return;
            }
        }
    });
}

BrowserTab *TabPage::activePane() const
{
    return m_active;
}

QList<BrowserTab *> TabPage::panes() const
{
    QList<BrowserTab *> found;
    for (int i = 0; i < count(); ++i) {
        if (auto *pane = qobject_cast<BrowserTab *>(widget(i))) {
            found.append(pane);
        }
    }
    return found;
}

bool TabPage::isSplit() const
{
    return count() > 1;
}

void TabPage::split(BrowserTab *second)
{
    insertWidget(indexOf(m_active) + 1, second);
    // Half and half, whatever the window's width.
    const int half = width() / 2;
    setSizes({half, width() - half});
    setActivePane(second);
    second->focusView();
}

void TabPage::unsplit()
{
    for (BrowserTab *pane : panes()) {
        if (pane != m_active) {
            pane->hide();
            pane->deleteLater();
            pane->setParent(nullptr);
        }
    }
}

void TabPage::setActivePane(BrowserTab *pane)
{
    if (pane != m_active) {
        m_active = pane;
        Q_EMIT activePaneChanged();
    }
}

} // namespace fe
