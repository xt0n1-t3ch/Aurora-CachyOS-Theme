// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements commandpalette.h.

#include "commandpalette.h"

#include "core/format.h"
#include "core/palette.h"
#include "style.h"

#include <QAction>
#include <QApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>

namespace fe {

namespace {

constexpr int WIDTH = 560;
constexpr int TOP = 64;
constexpr int PADDING = 8;
constexpr int FIELD_HEIGHT = 34;
constexpr int ROW_HEIGHT = 30;
constexpr int VISIBLE_ROWS = 10;
constexpr int ICON_SIZE = 16;
constexpr qreal ROW_RADIUS = 7.0;

/// A menu text as a reader sees it: without the accelerator's ampersand and the ellipsis.
QString plainText(const QAction *action)
{
    QString text = action->text();
    text.remove(QLatin1Char('&'));
    if (text.endsWith(QChar(0x2026))) {
        text.chop(1);
    }
    return text;
}

} // namespace

CommandPalette::CommandPalette(QWidget *window)
    : QWidget(window)
    , m_field(new QLineEdit(this))
{
    setAccessibleName(QStringLiteral("Command palette"));
    m_field->setFrame(false);
    m_field->setFont(style::uiFont(14));
    m_field->setPlaceholderText(QStringLiteral("Type a command"));
    m_field->setAccessibleName(QStringLiteral("Command"));
    QPalette colors = m_field->palette();
    colors.setColor(QPalette::Base, Qt::transparent);
    colors.setColor(QPalette::Text, style::TEXT);
    colors.setColor(QPalette::PlaceholderText, style::TEXT_SECONDARY);
    m_field->setPalette(colors);
    m_field->setTextMargins(PADDING + 4, 0, PADDING, 0);
    m_field->installEventFilter(this);
    connect(m_field, &QLineEdit::textChanged, this, &CommandPalette::refresh);
    setMouseTracking(true);
    hide();
}

void CommandPalette::open(const QList<Command> &commands)
{
    m_commands = commands;
    m_field->clear();
    refresh();
    place();
    show();
    raise();
    m_field->setFocus(Qt::ShortcutFocusReason);
    // A press anywhere else in the window closes the palette, as a click outside a menu closes it.
    qApp->installEventFilter(this);
}

void CommandPalette::close()
{
    if (isHidden()) {
        return;
    }
    qApp->removeEventFilter(this);
    hide();
    m_commands.clear();
    m_shown.clear();
    Q_EMIT closed();
}

QString CommandPalette::chosenText() const
{
    if (m_chosen >= m_shown.size()) {
        return {};
    }
    const QAction *action = m_commands.at(m_shown.at(m_chosen)).action;
    return action ? plainText(action) : QString();
}

QStringList CommandPalette::shownTexts() const
{
    QStringList texts;
    for (const qsizetype index : m_shown) {
        if (const QAction *action = m_commands.at(index).action) {
            texts.append(plainText(action));
        }
    }
    return texts;
}

void CommandPalette::refresh()
{
    QList<PaletteEntry> entries;
    entries.reserve(m_commands.size());
    for (const Command &command : std::as_const(m_commands)) {
        entries.append({command.action ? plainText(command.action) : QString(), command.place});
    }
    m_shown = rankPalette(m_field->text(), entries);
    m_chosen = 0;
    m_top = 0;
    place();
    update();
}

void CommandPalette::place()
{
    const QWidget *window = parentWidget();
    const int rows = int(std::min<qsizetype>(m_shown.size(), VISIBLE_ROWS));
    const int height = PADDING + FIELD_HEIGHT + (rows > 0 ? PADDING : 0) + rows * ROW_HEIGHT + PADDING;
    const int width = std::min(WIDTH, window->width() - 2 * PADDING);
    setGeometry((window->width() - width) / 2, TOP, width, height);
    m_field->setGeometry(PADDING, PADDING, width - 2 * PADDING, FIELD_HEIGHT);
}

QRectF CommandPalette::rowRect(qsizetype visibleRow) const
{
    const qreal y = PADDING + FIELD_HEIGHT + PADDING + qreal(visibleRow) * ROW_HEIGHT;
    return {qreal(PADDING), y, qreal(width() - 2 * PADDING), qreal(ROW_HEIGHT)};
}

qsizetype CommandPalette::rowAt(QPointF pos) const
{
    for (qsizetype row = 0; row < VISIBLE_ROWS && m_top + row < m_shown.size(); ++row) {
        if (rowRect(row).contains(pos)) {
            return m_top + row;
        }
    }
    return -1;
}

void CommandPalette::run(qsizetype row)
{
    if (row < 0 || row >= m_shown.size()) {
        return;
    }
    const QPointer<QAction> action = m_commands.at(m_shown.at(row)).action;
    close();
    // The palette is gone before the command runs, so a command that opens a dialog or moves the focus keeps it.
    if (action && action->isEnabled()) {
        action->trigger();
    }
}

bool CommandPalette::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_field && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        const qsizetype count = m_shown.size();
        switch (key->key()) {
        case Qt::Key_Escape:
            close();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            run(m_chosen);
            return true;
        case Qt::Key_Down:
        case Qt::Key_Up:
            if (count > 0) {
                m_chosen = (m_chosen + (key->key() == Qt::Key_Down ? 1 : count - 1)) % count;
                // Keep the chosen row in sight.
                m_top = std::clamp(m_top, m_chosen - VISIBLE_ROWS + 1, m_chosen);
                update();
            }
            return true;
        default:
            break;
        }
    }
    if (event->type() == QEvent::MouseButtonPress && isVisible()) {
        auto *widget = qobject_cast<QWidget *>(watched);
        if (widget && widget != this && !isAncestorOf(widget) && widget->window() == window()) {
            close();
        }
    }
    if (watched == window() && event->type() == QEvent::Resize && isVisible()) {
        place();
    }
    return QWidget::eventFilter(watched, event);
}

void CommandPalette::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    style::paintGlass(painter, QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), style::MENU_RADIUS, style::MENU_FILL);
    style::paintGlass(painter, QRectF(m_field->geometry()), FIELD_HEIGHT / 2.0, style::CAPSULE_FILL);
    const QFont text = style::uiFont(13);
    const QFont small = style::uiFont(12);
    for (qsizetype row = 0; row < VISIBLE_ROWS && m_top + row < m_shown.size(); ++row) {
        const qsizetype index = m_top + row;
        const Command &command = m_commands.at(m_shown.at(index));
        if (!command.action) {
            continue;
        }
        const QRectF box = rowRect(row);
        const bool chosen = index == m_chosen;
        if (chosen) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(style::SELECTION);
            painter.drawRoundedRect(box, ROW_RADIUS, ROW_RADIUS);
        }
        const bool enabled = command.action->isEnabled();
        const QColor ink = !enabled ? style::TEXT_TERTIARY : style::TEXT;
        qreal x = box.left() + 10;
        const QIcon icon = command.action->icon();
        if (!icon.isNull()) {
            const QPixmap pixmap = icon.pixmap(QSize(ICON_SIZE, ICON_SIZE), devicePixelRatioF(), enabled ? QIcon::Normal : QIcon::Disabled);
            painter.drawPixmap(QPointF(x, box.center().y() - ICON_SIZE / 2.0), pixmap);
        }
        x += ICON_SIZE + 10;
        QString label = plainText(command.action);
        if (command.action->isCheckable() && command.action->isChecked()) {
            label += QStringLiteral("  \u2713");
        }
        painter.setFont(text);
        painter.setPen(ink);
        const qreal labelWidth = QFontMetricsF(text).horizontalAdvance(label);
        painter.drawText(QRectF(x, box.top(), box.width(), box.height()), Qt::AlignVCenter | Qt::AlignLeft, label);
        painter.setFont(small);
        painter.setPen(chosen ? style::TEXT : style::TEXT_SECONDARY);
        painter.drawText(QRectF(x + labelWidth + 10, box.top(), box.width(), box.height()), Qt::AlignVCenter | Qt::AlignLeft, command.place);
        const QKeySequence keys = command.action->shortcut();
        if (!keys.isEmpty()) {
            painter.drawText(box.adjusted(0, 0, -10, 0), Qt::AlignVCenter | Qt::AlignRight, shortcutLabel(keys.toString(QKeySequence::PortableText)));
        }
    }
}

void CommandPalette::mousePressEvent(QMouseEvent *event)
{
    run(rowAt(event->position()));
}

void CommandPalette::mouseMoveEvent(QMouseEvent *event)
{
    const qsizetype row = rowAt(event->position());
    if (row >= 0 && row != m_chosen) {
        m_chosen = row;
        update();
    }
}

} // namespace fe
