// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The row of file actions under the toolbar: new items, trash, rename, duplicate, Quick Look, the clipboard, the
// terminal and Split View, one click away.

#pragma once

#include <QHash>
#include <QWidget>

class QHBoxLayout;
class QSpacerItem;

namespace fe {

class GlassButton;

/// What a button of the action row does.
enum class FileAction {
    NewFolder,
    NewFile,
    MoveToTrash,
    Rename,
    Duplicate,
    QuickLook,
    Cut,
    Copy,
    Paste,
    OpenTerminal,
    SplitView,
};

/// A glass strip of icon buttons in groups, with Open Terminal Here and Split View at its right end.
class ActionBar : public QWidget
{
    Q_OBJECT

public:
    explicit ActionBar(QWidget *parent = nullptr);
    /// Dims or enables the button of action.
    void setActionEnabled(FileAction action, bool enabled);
    /// Shows whether the window is split, on the Split View button.
    void setSplit(bool split);
    /// Puts widget in the row's open middle, right of the editing buttons (the transfer center's button).
    void addMiddleWidget(QWidget *widget);
    /// The button of action, for a menu to drop down below it.
    [[nodiscard]] GlassButton *button(FileAction action) const;

Q_SIGNALS:
    void triggered(fe::FileAction action);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    GlassButton *add(FileAction action, const QString &icon, const QString &toolTip, const QString &label = {});
    /// Draws a hairline before the next button added.
    void addDivider();

    QHBoxLayout *m_layout;
    /// The place of the stretch between the editing buttons and the ones at the right end.
    int m_middle = 0;
    QHash<FileAction, GlassButton *> m_buttons;
    /// The gaps that draw a hairline between groups.
    QList<QSpacerItem *> m_dividers;
};

} // namespace fe
