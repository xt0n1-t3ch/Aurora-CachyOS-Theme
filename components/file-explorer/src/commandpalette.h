// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The command palette: every command of the window's menus, found by a few typed letters and run with Enter. The
// ranking is in core/palette.

#pragma once

#include <QPointer>
#include <QWidget>

class QAction;
class QLineEdit;

namespace fe {

/// A glass panel over the top of the window: a field and the commands it finds, best first, each with where it lives
/// in the menus and its shortcut. Up and Down move, Enter or a click runs the command, Escape or a click elsewhere
/// closes it. It holds no commands of its own: open() takes the window's actions as they are at that moment.
class CommandPalette : public QWidget
{
    Q_OBJECT

public:
    /// A command: the action and the menu path it was found under ("View", "View > Sort By").
    struct Command {
        QPointer<QAction> action;
        QString place;
    };

    explicit CommandPalette(QWidget *window);

    /// Shows the palette over its window with commands, an empty field and the first command chosen.
    void open(const QList<Command> &commands);
    /// The text of the chosen command, or an empty string when nothing matches.
    [[nodiscard]] QString chosenText() const;
    /// The texts of the commands shown, best first.
    [[nodiscard]] QStringList shownTexts() const;

Q_SIGNALS:
    /// The palette closed; the window gives the keyboard back to its view.
    void closed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    void refresh();
    void place();
    void run(qsizetype row);
    void close();
    [[nodiscard]] QRectF rowRect(qsizetype visibleRow) const;
    [[nodiscard]] qsizetype rowAt(QPointF pos) const;

    QLineEdit *m_field;
    QList<Command> m_commands;
    /// Positions in m_commands, best first.
    QList<qsizetype> m_shown;
    qsizetype m_chosen = 0;
    /// The first shown command in the visible rows.
    qsizetype m_top = 0;
};

} // namespace fe
