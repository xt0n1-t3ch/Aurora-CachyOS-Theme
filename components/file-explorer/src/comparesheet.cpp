// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements comparesheet.h.

#include "comparesheet.h"

#include "core/inspector.h"
#include "style.h"

#include <KIO/CopyJob>
#include <KIO/FileUndoManager>
#include <KIO/JobUiDelegateFactory>
#include <KJobWidgets>

#include <QApplication>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QThread>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace fe {

namespace {

QString statusText(const CompareRow &row)
{
    switch (row.status) {
    case CompareStatus::OnlyLeft:
        return QStringLiteral("Only on the left");
    case CompareStatus::OnlyRight:
        return QStringLiteral("Only on the right");
    case CompareStatus::Different:
        return row.newer > 0 ? QStringLiteral("Newer on the left") : row.newer < 0 ? QStringLiteral("Newer on the right") : QStringLiteral("Different");
    case CompareStatus::Same:
        break;
    }
    return QStringLiteral("The same");
}

QColor statusColor(CompareStatus status)
{
    switch (status) {
    case CompareStatus::OnlyLeft:
    case CompareStatus::OnlyRight:
        return style::ACCENT;
    case CompareStatus::Different:
        return style::CAPACITY_LOW;
    case CompareStatus::Same:
        break;
    }
    return style::TEXT_SECONDARY;
}

} // namespace

CompareSheet::CompareSheet(const QUrl &left, const QUrl &right, QWidget *parent)
    : QDialog(parent)
    , m_left(left.toLocalFile())
    , m_right(right.toLocalFile())
    , m_list(new QTreeWidget(this))
    , m_summary(new QLabel(this))
    , m_contents(new QCheckBox(QStringLiteral("Compare contents"), this))
    , m_showSame(new QCheckBox(QStringLiteral("Show items that are the same"), this))
    , m_toRight(new QPushButton(QIcon::fromTheme(QStringLiteral("go-next")), QStringLiteral("Copy to Right"), this))
    , m_toLeft(new QPushButton(QIcon::fromTheme(QStringLiteral("go-previous")), QStringLiteral("Copy to Left"), this))
{
    setWindowTitle(QStringLiteral("Compare %1 and %2").arg(left.fileName(), right.fileName()));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(760, 520);
    auto *layout = new QVBoxLayout(this);
    auto *heads = new QHBoxLayout;
    heads->addWidget(new QLabel(QStringLiteral("Left: %1").arg(m_left), this), 1);
    heads->addWidget(new QLabel(QStringLiteral("Right: %1").arg(m_right), this), 1);
    layout->addLayout(heads);
    m_list->setColumnCount(2);
    m_list->setHeaderLabels({QStringLiteral("Item"), QStringLiteral("Difference")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_list->setAccessibleName(QStringLiteral("Differences"));
    layout->addWidget(m_list, 1);
    auto *options = new QHBoxLayout;
    options->addWidget(m_contents);
    options->addWidget(m_showSame);
    options->addStretch();
    layout->addLayout(options);
    layout->addWidget(m_summary);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addButton(m_toLeft, QDialogButtonBox::ActionRole);
    buttons->addButton(m_toRight, QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    connect(m_contents, &QCheckBox::toggled, this, &CompareSheet::compare);
    connect(m_showSame, &QCheckBox::toggled, this, [this] { showRows(m_rows); });
    connect(m_toRight, &QPushButton::clicked, this, [this] { copyAcross(true); });
    connect(m_toLeft, &QPushButton::clicked, this, [this] { copyAcross(false); });
    connect(m_list, &QTreeWidget::itemSelectionChanged, this, [this] {
        const bool any = !m_list->selectedItems().isEmpty();
        m_toRight->setEnabled(any);
        m_toLeft->setEnabled(any);
    });
    m_toRight->setEnabled(false);
    m_toLeft->setEnabled(false);
    compare();
}

CompareSheet::~CompareSheet()
{
    if (m_cancel) {
        *m_cancel = true;
    }
}

const QList<CompareRow> &CompareSheet::rows() const
{
    return m_rows;
}

void CompareSheet::compare()
{
    if (m_cancel) {
        *m_cancel = true;
    }
    auto cancel = std::make_shared<std::atomic_bool>(false);
    m_cancel = cancel;
    m_summary->setText(QStringLiteral("Comparing"));
    const QString left = m_left;
    const QString right = m_right;
    const bool contents = m_contents->isChecked();
    QPointer<CompareSheet> sheet(this);
    QThread *thread = QThread::create([left, right, contents, cancel, sheet] {
        const QHash<QString, ItemFacts> leftItems = walkFolder(left, *cancel);
        const QHash<QString, ItemFacts> rightItems = walkFolder(right, *cancel);
        // Contents decide only for files of the same size: both are read and their SHA-256 compared.
        const auto sameContents = [&](const QString &path) {
            return hashFile(left + u'/' + path, QCryptographicHash::Sha256, *cancel)
                == hashFile(right + u'/' + path, QCryptographicHash::Sha256, *cancel);
        };
        QList<CompareRow> rows = compareFolders(leftItems, rightItems, contents ? std::function<bool(const QString &)>(sameContents) : nullptr);
        QMetaObject::invokeMethod(qApp, [sheet, cancel, rows = std::move(rows)] {
            if (sheet && !*cancel && sheet->m_cancel == cancel) {
                sheet->showRows(rows);
                Q_EMIT sheet->compared();
            }
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start(QThread::IdlePriority);
}

void CompareSheet::showRows(const QList<CompareRow> &rows)
{
    m_rows = rows;
    m_list->setUpdatesEnabled(false);
    m_list->clear();
    QList<QTreeWidgetItem *> lines;
    for (const CompareRow &row : rows) {
        if (row.status == CompareStatus::Same && !m_showSame->isChecked()) {
            continue;
        }
        auto *line = new QTreeWidgetItem({row.path, statusText(row)});
        line->setData(0, Qt::UserRole, row.path);
        line->setForeground(1, statusColor(row.status));
        lines.append(line);
    }
    m_list->addTopLevelItems(lines);
    m_list->setUpdatesEnabled(true);
    m_summary->setText(compareSummary(rows));
}

void CompareSheet::copyAcross(bool toRight)
{
    const QString from = toRight ? m_left : m_right;
    const QString to = toRight ? m_right : m_left;
    QStringList paths;
    for (const QTreeWidgetItem *line : m_list->selectedItems()) {
        paths.append(line->data(0, Qt::UserRole).toString());
    }
    // A folder brings what is in it: paths inside a chosen folder would be copied twice.
    std::sort(paths.begin(), paths.end());
    QStringList outermost;
    for (const QString &path : std::as_const(paths)) {
        if (outermost.isEmpty() || !path.startsWith(outermost.constLast() + u'/')) {
            outermost.append(path);
        }
    }
    for (const QString &path : std::as_const(outermost)) {
        const QUrl source = QUrl::fromLocalFile(from + u'/' + path);
        if (!QFileInfo::exists(source.toLocalFile())) {
            continue;
        }
        const QUrl target = QUrl::fromLocalFile(to + u'/' + path);
        QDir().mkpath(QFileInfo(target.toLocalFile()).absolutePath());
        // KIO asks before overwriting a different item on the other side, and the copy can be undone.
        KIO::CopyJob *job = KIO::copyAs(source, target);
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, this));
        KJobWidgets::setWindow(job, this);
        KIO::FileUndoManager::self()->recordCopyJob(job);
        connect(job, &KJob::result, this, &CompareSheet::compare);
    }
}

} // namespace fe
