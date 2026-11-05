// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements actionbar.h.

#include "actionbar.h"

#include "style.h"
#include "toolbar.h"

#include <QHBoxLayout>
#include <QPainter>

namespace fe {

namespace {

constexpr int BAR_HEIGHT = 44;
constexpr int BAR_RADIUS = 13;

QString symbol(const char *name)
{
    return QStringLiteral(":/icons/actions/%1.svg").arg(QLatin1String(name));
}

} // namespace

ActionBar::ActionBar(QWidget *parent)
    : QWidget(parent)
    , m_layout(new QHBoxLayout(this))
{
    setFixedHeight(BAR_HEIGHT);
    m_layout->setContentsMargins(10, 0, 10, 0);
    m_layout->setSpacing(4);

    add(FileAction::NewFolder, symbol("folder-new"), QStringLiteral("New Folder (Ctrl+Shift+N)"));
    add(FileAction::NewFile, symbol("file-new"), QStringLiteral("New File"));
    addDivider();
    add(FileAction::MoveToTrash, symbol("trash"), QStringLiteral("Move to Trash (Del)"));
    add(FileAction::Rename, symbol("rename"), QStringLiteral("Rename (F2)"));
    add(FileAction::Duplicate, symbol("duplicate"), QStringLiteral("Duplicate (Ctrl+D)"));
    add(FileAction::QuickLook, symbol("quick-look"), QStringLiteral("Quick Look (Space)"));
    addDivider();
    add(FileAction::Cut, symbol("cut"), QStringLiteral("Cut (Ctrl+X)"));
    add(FileAction::Copy, symbol("copy"), QStringLiteral("Copy (Ctrl+C)"));
    add(FileAction::Paste, symbol("paste"), QStringLiteral("Paste (Ctrl+V)"));
    m_layout->addStretch(1);
    add(FileAction::OpenTerminal, symbol("terminal"), QStringLiteral("Open a terminal in this folder"),
        QStringLiteral("Open Terminal Here"));
    addDivider();
    add(FileAction::SplitView, symbol("split-view"), QStringLiteral("Show two folders side by side (F3)"), QStringLiteral("Split View"))
        ->setCheckable(true);
}

GlassButton *ActionBar::add(FileAction action, const QString &icon, const QString &toolTip, const QString &label)
{
    auto *button = new GlassButton(icon, toolTip, this);
    if (!label.isEmpty()) {
        button->setLabel(label);
    }
    connect(button, &QAbstractButton::clicked, this, [this, action] { Q_EMIT triggered(action); });
    m_layout->addWidget(button);
    m_buttons.insert(action, button);
    return button;
}

void ActionBar::addDivider()
{
    auto *gap = new QSpacerItem(13, 1, QSizePolicy::Fixed, QSizePolicy::Minimum);
    m_layout->addSpacerItem(gap);
    m_dividers.append(gap);
}

void ActionBar::setActionEnabled(FileAction action, bool enabled)
{
    if (GlassButton *target = m_buttons.value(action)) {
        target->setEnabled(enabled);
    }
}

void ActionBar::setSplit(bool split)
{
    m_buttons.value(FileAction::SplitView)->setChecked(split);
}

GlassButton *ActionBar::button(FileAction action) const
{
    return m_buttons.value(action);
}

void ActionBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    style::paintGlass(painter, bounds, BAR_RADIUS, style::CAPSULE_FILL);

    painter.setPen(QPen(style::HAIRLINE, 1.0));
    for (const QSpacerItem *gap : std::as_const(m_dividers)) {
        const qreal x = gap->geometry().center().x() + 0.5;
        painter.drawLine(QPointF(x, 12), QPointF(x, height() - 12));
    }
}

} // namespace fe
