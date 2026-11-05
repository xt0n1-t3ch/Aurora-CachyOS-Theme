// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements chooserbar.h.

#include "chooserbar.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

namespace fe {

ChooserBar::ChooserBar(bool saving, const QString &acceptLabel, const QStringList &filterLabels, int currentFilter,
                       const QString &name, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 10, 16, 12);
    layout->setSpacing(10);

    if (saving) {
        layout->addWidget(new QLabel(QStringLiteral("Save As:"), this));
        m_name = new QLineEdit(name, this);
        m_name->setMinimumWidth(260);
        // Like macOS, the name is selected without its extension, ready to type over.
        const int dot = name.lastIndexOf(QLatin1Char('.'));
        m_name->setSelection(0, dot > 0 ? dot : name.size());
        connect(m_name, &QLineEdit::returnPressed, this, &ChooserBar::accepted);
        layout->addWidget(m_name);
    }
    if (filterLabels.size() > 1 || (!saving && !filterLabels.isEmpty())) {
        m_filter = new QComboBox(this);
        m_filter->addItems(filterLabels);
        m_filter->setCurrentIndex(qBound(0, currentFilter, int(filterLabels.size()) - 1));
        connect(m_filter, &QComboBox::currentIndexChanged, this, &ChooserBar::filterChanged);
        layout->addWidget(m_filter);
    }
    layout->addStretch(1);

    auto *cancel = new QPushButton(QStringLiteral("Cancel"), this);
    connect(cancel, &QPushButton::clicked, this, &ChooserBar::rejected);
    layout->addWidget(cancel);

    m_accept = new QPushButton(acceptLabel, this);
    m_accept->setDefault(true);
    m_accept->setAutoDefault(true);
    connect(m_accept, &QPushButton::clicked, this, &ChooserBar::accepted);
    layout->addWidget(m_accept);
}

QString ChooserBar::name() const
{
    return m_name ? m_name->text() : QString();
}

void ChooserBar::setName(const QString &name)
{
    if (m_name) {
        m_name->setText(name);
    }
}

int ChooserBar::currentFilter() const
{
    return m_filter ? m_filter->currentIndex() : 0;
}

} // namespace fe
