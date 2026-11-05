// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The toolbar widgets: glass buttons and capsules, the search field, and the toolbar that lays them out.

#pragma once

#include <QAbstractButton>
#include <QLineEdit>
#include <QWidget>

class QButtonGroup;
class QHBoxLayout;

namespace fe {

class TitleLabel;

/// How a tab lays out its files; the values are the ids of the toolbar's view buttons.
enum class ViewMode {
    Icons,
    List,
    Compact,
};

/// An icon button of the macOS 27 toolbar. Alone it is a round glass button; inside a Capsule it
/// only draws its hover, pressed and selected states.
class GlassButton : public QAbstractButton
{
    Q_OBJECT

public:
    /// iconName is a theme icon name, or a resource path starting with ":/" for the app's own symbols.
    GlassButton(QString iconName, const QString &toolTip, QWidget *parent = nullptr);
    /// Draws the round glass behind the button (true) or only its states (false).
    void setStandalone(bool standalone);
    void setIconName(const QString &iconName);
    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QString m_iconName;
    bool m_standalone = false;
    bool m_hover = false;
};

/// A glass pill that groups toolbar buttons, like the back/forward and view mode groups of Finder.
class Capsule : public QWidget
{
    Q_OBJECT

public:
    explicit Capsule(QWidget *parent = nullptr);
    void addButton(GlassButton *button);
    /// Draws a hairline between the last button added and the next one.
    void addDivider();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QHBoxLayout *m_layout;
    QList<int> m_dividers;
};

/// The round search field at the right end of the toolbar.
class SearchField : public QLineEdit
{
    Q_OBJECT

public:
    explicit SearchField(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
};

/// The unified toolbar: navigation, the folder title, view controls and search.
class Toolbar : public QWidget
{
    Q_OBJECT

public:
    explicit Toolbar(QWidget *parent = nullptr);

    void setTitle(const QString &title);
    /// Enables or dims the back, forward and enclosing folder buttons.
    void setNavigationState(bool canGoBack, bool canGoForward, bool canGoUp);
    /// Checks the button of mode without sending viewModeChosen.
    void setViewMode(ViewMode mode);
    /// Shows term in the search field without reporting it as typed.
    void setSearchText(const QString &term);
    /// Dims the Add to Sidebar button when the sidebar shows everything it would add.
    void setAddToSidebarEnabled(bool enabled);
    /// The slot left of the navigation buttons, where the window buttons go when the sidebar is hidden.
    [[nodiscard]] QHBoxLayout *leadingLayout() const;
    /// The slot right of the search field, where the window buttons go.
    [[nodiscard]] QHBoxLayout *trailingLayout() const;
    [[nodiscard]] SearchField *searchField() const;
    [[nodiscard]] GlassButton *sortButton() const;

Q_SIGNALS:
    void backClicked();
    void forwardClicked();
    /// The enclosing folder button was clicked.
    void upClicked();
    void newFolderClicked();
    /// The user clicked a view mode button.
    void viewModeChosen(fe::ViewMode mode);
    void sortClicked();
    void addToSidebarClicked();
    /// The user typed in or cleared the search field.
    void searchChanged(const QString &term);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    /// Makes everything fit the toolbar's width, giving up space in Finder's order: the search field
    /// narrows first, then the least used buttons fold away, and the folder title goes last.
    void fitToWidth();

    QHBoxLayout *m_leading;
    QHBoxLayout *m_trailing;
    GlassButton *m_back;
    GlassButton *m_forward;
    GlassButton *m_up;
    TitleLabel *m_title;
    QButtonGroup *m_viewModes;
    GlassButton *m_sort;
    SearchField *m_search;
    GlassButton *m_addToSidebar = nullptr;
    /// The buttons that fold away when the window is narrow, the first to go first.
    QList<QWidget *> m_collapsible;
};

} // namespace fe
