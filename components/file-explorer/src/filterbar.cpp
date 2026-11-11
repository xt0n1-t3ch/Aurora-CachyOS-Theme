// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements filterbar.h.

#include "filterbar.h"

#include "core/format.h"
#include "style.h"
#include "toolbar.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>

namespace fe {

namespace {

/// The same height as the search scope row, so the view does not jump when one replaces the other.
constexpr int BAR_HEIGHT = 34;
constexpr int FIELD_WIDTH = 260;
constexpr int FIELD_HEIGHT = 24;
constexpr int ICON_SIZE = 13;

/// The glass capsule of the field, with the funnel at its left.
class FilterField : public QLineEdit
{
public:
    explicit FilterField(QWidget *parent)
        : QLineEdit(parent)
    {
        setFrame(false);
        setFont(style::uiFont(12));
        setFixedSize(FIELD_WIDTH, FIELD_HEIGHT);
        setTextMargins(26, 0, 10, 0);
        setPlaceholderText(QStringLiteral("Filter by name"));
        setClearButtonEnabled(true);
        QPalette colors = palette();
        colors.setColor(QPalette::Base, Qt::transparent);
        colors.setColor(QPalette::Text, style::TEXT);
        colors.setColor(QPalette::PlaceholderText, style::TEXT_SECONDARY);
        setPalette(colors);
        setAccessibleName(QStringLiteral("Filter by name"));
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        {
            QPainter painter(this);
            const QRectF bounds(rect());
            style::paintGlass(painter, bounds, bounds.height() / 2.0, hasFocus() ? style::CAPSULE_HOVER : style::CAPSULE_FILL);
            if (hasFocus()) {
                style::paintFocusRing(painter, bounds.adjusted(1, 1, -1, -1));
            }
            const QPixmap funnel = style::tintedIcon(QStringLiteral("view-filter-symbolic"), ICON_SIZE, style::TEXT_SECONDARY, devicePixelRatioF());
            painter.drawPixmap(QPointF(9, (height() - ICON_SIZE) / 2.0), funnel);
        }
        QLineEdit::paintEvent(event);
    }
};

} // namespace

FilterBar::FilterBar(QWidget *parent)
    : QWidget(parent)
    , m_field(new FilterField(this))
    , m_count(new QLabel(this))
    , m_close(new GlassButton(QStringLiteral("window-close-symbolic"), QStringLiteral("Close the filter (Esc)"), this))
{
    setFixedHeight(BAR_HEIGHT);
    setAccessibleName(QStringLiteral("Filter"));
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 0, 10, 0);
    layout->setSpacing(10);
    layout->addWidget(m_field);
    layout->addWidget(m_count);
    layout->addStretch();
    layout->addWidget(m_close);
    m_count->setFont(style::uiFont(12));
    QPalette countColors = m_count->palette();
    countColors.setColor(QPalette::WindowText, style::TEXT_SECONDARY);
    m_count->setPalette(countColors);
    m_close->setFixedSize(FIELD_HEIGHT, FIELD_HEIGHT);
    m_field->installEventFilter(this);
    connect(m_field, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (text.isEmpty()) {
            m_count->clear();
        }
        Q_EMIT filterChanged(text);
    });
    connect(m_close, &QAbstractButton::clicked, this, [this] {
        dismiss();
        Q_EMIT focusViewRequested();
    });
    hide();
}

void FilterBar::open()
{
    show();
    m_field->setFocus(Qt::ShortcutFocusReason);
    m_field->selectAll();
}

void FilterBar::dismiss()
{
    // clear() sends textChanged only when there was text, so an empty row closes without a signal.
    m_field->clear();
    hide();
}

QString FilterBar::text() const
{
    return m_field->text();
}

void FilterBar::setMatchCount(qsizetype matches)
{
    m_count->setText(m_field->text().isEmpty() ? QString() : filterMatchText(matches));
}

QString FilterBar::matchText() const
{
    return m_count->text();
}

bool FilterBar::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_field && event->type() == QEvent::KeyPress) {
        switch (static_cast<QKeyEvent *>(event)->key()) {
        case Qt::Key_Escape:
            dismiss();
            Q_EMIT focusViewRequested();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
        case Qt::Key_Down:
            Q_EMIT focusViewRequested();
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace fe
