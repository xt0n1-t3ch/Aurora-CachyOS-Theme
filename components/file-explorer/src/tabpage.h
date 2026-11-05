// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// What one tab of the window holds: one folder view, or two side by side in Split View.

#pragma once

#include <QPointer>
#include <QSplitter>

namespace fe {

class BrowserTab;

/// The page of a tab: a BrowserTab, and a second one beside it while split. The pane the user last clicked or
/// typed in is the active one; the toolbar, the path bar and the menus follow it.
class TabPage : public QSplitter
{
    Q_OBJECT

public:
    /// Takes primary as the only pane.
    explicit TabPage(BrowserTab *primary, QWidget *parent = nullptr);

    [[nodiscard]] BrowserTab *activePane() const;
    [[nodiscard]] QList<BrowserTab *> panes() const;
    [[nodiscard]] bool isSplit() const;
    /// Puts second right of the active pane and makes it active.
    void split(BrowserTab *second);
    /// Closes the pane that is not active.
    void unsplit();

Q_SIGNALS:
    /// The other pane became the active one.
    void activePaneChanged();

private:
    void setActivePane(BrowserTab *pane);

    QPointer<BrowserTab> m_active;
};

} // namespace fe
