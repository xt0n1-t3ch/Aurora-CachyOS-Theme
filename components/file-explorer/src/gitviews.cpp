// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements gitviews.h.

#include "gitviews.h"

#include "style.h"

#include <QFontDatabase>
#include <QHeaderView>
#include <QSyntaxHighlighter>

namespace fe {

namespace {

/// The most of a diff shown at once; a longer one is cut there with a note, so a huge generated file cannot stall the
/// window.
constexpr qsizetype DIFF_LIMIT = 2 * 1024 * 1024;
constexpr int MONO_PIXEL_SIZE = 12;

class DiffHighlighter : public QSyntaxHighlighter
{
public:
    using QSyntaxHighlighter::QSyntaxHighlighter;

protected:
    void highlightBlock(const QString &line) override
    {
        QTextCharFormat format;
        if (line.startsWith(QLatin1String("+++")) || line.startsWith(QLatin1String("---")) || line.startsWith(QLatin1String("diff ")) ||
            line.startsWith(QLatin1String("index ")) || line.startsWith(QLatin1String("new file")) || line.startsWith(QLatin1String("deleted file")) ||
            line.startsWith(QLatin1String("similarity ")) || line.startsWith(QLatin1String("rename ")) || line.startsWith(QLatin1String("old mode")) ||
            line.startsWith(QLatin1String("new mode"))) {
            format.setForeground(style::TEXT_SECONDARY);
            format.setFontWeight(QFont::DemiBold);
        } else if (line.startsWith(QLatin1String("@@"))) {
            format.setForeground(style::ACCENT);
        } else if (line.startsWith(QLatin1Char('+'))) {
            format.setForeground(style::GIT_ADDED);
            format.setBackground(style::DIFF_ADDED_FILL);
        } else if (line.startsWith(QLatin1Char('-'))) {
            format.setForeground(style::GIT_DELETED);
            format.setBackground(style::DIFF_REMOVED_FILL);
        } else {
            return;
        }
        setFormat(0, int(line.size()), format);
    }
};

QColor letterColor(QChar letter)
{
    switch (letter.unicode()) {
    case 'U':
    case 'A':
        return style::GIT_ADDED;
    case 'D':
    case '!':
        return style::GIT_DELETED;
    case 'R':
        return style::GIT_RENAMED;
    default:
        return style::GIT_MODIFIED;
    }
}

} // namespace

DiffView::DiffView(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setReadOnly(true);
    setLineWrapMode(QPlainTextEdit::NoWrap);
    setFrameShape(QFrame::NoFrame);
    QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    mono.setPixelSize(MONO_PIXEL_SIZE);
    setFont(mono);
    setAccessibleName(QStringLiteral("Changes"));
    new DiffHighlighter(document());
}

void DiffView::setDiff(const QByteArray &diff, const QString &emptyText)
{
    if (diff.trimmed().isEmpty()) {
        setPlainText(emptyText);
        return;
    }
    QString text = QString::fromUtf8(diff.left(DIFF_LIMIT));
    if (diff.size() > DIFF_LIMIT) {
        text += QStringLiteral("\n\nThe rest of these changes is too long to show here.");
    }
    setPlainText(text);
}

ChangesList::ChangesList(bool checkable, QWidget *parent)
    : QTreeWidget(parent)
    , m_checkable(checkable)
{
    setColumnCount(2);
    setHeaderHidden(true);
    setRootIsDecorated(false);
    setUniformRowHeights(true);
    setFrameShape(QFrame::NoFrame);
    setAccessibleName(QStringLiteral("Changed files"));
    header()->setStretchLastSection(true);
    header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    connect(this, &QTreeWidget::currentItemChanged, this, &ChangesList::currentEntryChanged);
    connect(this, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *, int column) {
        if (column == 0) {
            Q_EMIT checksChanged();
        }
    });
}

void ChangesList::setEntries(const QList<git::StatusEntry> &entries)
{
    // The rows are built quietly; the signals go out once, after the blocker is gone, so the sheet shows the first
    // row's diff.
    QSignalBlocker blocker(this);
    clear();
    m_entries.clear();
    QFont letterFont = font();
    letterFont.setBold(true);
    for (const git::StatusEntry &entry : entries) {
        if (entry.kind == git::StatusEntry::Kind::Ignored) {
            continue;
        }
        const QChar letter = git::changeLetter(entry);
        auto *row = new QTreeWidgetItem(this);
        row->setText(0, letter);
        row->setFont(0, letterFont);
        row->setForeground(0, letterColor(letter));
        row->setToolTip(0, git::changeName(entry));
        row->setText(1, entry.originalPath.isEmpty() ? entry.path : QStringLiteral("%1   (was %2)").arg(entry.path, entry.originalPath));
        row->setToolTip(1, entry.path);
        row->setData(0, Qt::UserRole, m_entries.size());
        if (m_checkable) {
            row->setFlags(row->flags() | Qt::ItemIsUserCheckable);
            row->setCheckState(0, Qt::Checked);
        }
        m_entries.append(entry);
    }
    if (topLevelItemCount() > 0) {
        setCurrentItem(topLevelItem(0));
    }
    blocker.unblock();
    Q_EMIT currentEntryChanged();
    Q_EMIT checksChanged();
}

QList<git::StatusEntry> ChangesList::entries() const
{
    return m_entries;
}

QList<git::StatusEntry> ChangesList::checkedEntries() const
{
    QList<git::StatusEntry> checked;
    for (int i = 0; i < topLevelItemCount(); ++i) {
        const QTreeWidgetItem *row = topLevelItem(i);
        if (!m_checkable || row->checkState(0) == Qt::Checked) {
            checked.append(m_entries.at(row->data(0, Qt::UserRole).toInt()));
        }
    }
    return checked;
}

std::optional<git::StatusEntry> ChangesList::currentEntry() const
{
    if (const QTreeWidgetItem *row = currentItem()) {
        return m_entries.at(row->data(0, Qt::UserRole).toInt());
    }
    return std::nullopt;
}

void ChangesList::setAllChecked(bool checked, bool lockChecks)
{
    for (int i = 0; i < topLevelItemCount(); ++i) {
        QTreeWidgetItem *row = topLevelItem(i);
        row->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
        if (lockChecks) {
            row->setFlags(row->flags() & ~Qt::ItemIsUserCheckable);
        }
    }
}

} // namespace fe
