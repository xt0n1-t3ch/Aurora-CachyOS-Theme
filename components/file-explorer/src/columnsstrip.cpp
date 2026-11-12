// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements columnsstrip.h.

#include "columnsstrip.h"

#include "core/paths.h"
#include "style.h"

#include <KDirLister>
#include <KDirModel>
#include <KDirSortFilterProxyModel>
#include <KIO/DropJob>
#include <KJobWidgets>

#include <QDragMoveEvent>
#include <QHBoxLayout>
#include <QListView>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>

namespace fe {

namespace {

constexpr int COLUMN_WIDTH = 210;
constexpr int ICON_SIZE = 16;

} // namespace

/// One column: the listing of one folder, sorted as Dolphin sorts (folders first, natural order), with the next folder
/// on the way marked.
class FolderColumn : public QListView
{
public:
    FolderColumn(const QUrl &folder, bool showHidden, QWidget *parent)
        : QListView(parent)
        , m_folder(folder)
        , m_model(new KDirModel(this))
        , m_proxy(new KDirSortFilterProxyModel(this))
    {
        setFixedWidth(COLUMN_WIDTH);
        setFrameShape(QFrame::NoFrame);
        setIconSize(QSize(ICON_SIZE, ICON_SIZE));
        setUniformItemSizes(true);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
        setDragEnabled(true);
        setAcceptDrops(true);
        setDropIndicatorShown(true);
        setDragDropMode(QAbstractItemView::DragDrop);
        setFont(style::uiFont(13));
        setAttribute(Qt::WA_TranslucentBackground);
        viewport()->setAutoFillBackground(false);
        QPalette colors = palette();
        colors.setColor(QPalette::Base, Qt::transparent);
        colors.setColor(QPalette::Text, style::TEXT);
        colors.setColor(QPalette::Highlight, style::SELECTION_INACTIVE);
        colors.setColor(QPalette::HighlightedText, style::TEXT);
        setPalette(colors);
        setAccessibleName(folder.fileName().isEmpty() ? folder.toLocalFile() : folder.fileName());
        m_proxy->setSourceModel(m_model);
        m_proxy->setSortFoldersFirst(true);
        m_proxy->sort(0);
        setModel(m_proxy);
        m_model->dirLister()->setShowHiddenFiles(showHidden);
        // KIO's own cache and watch keep the listing current; a listing done marks the next folder again.
        connect(m_model->dirLister(), &KCoreDirLister::completed, this, [this] { markNext(); });
        m_model->openUrl(folder);
    }

    [[nodiscard]] QUrl folder() const
    {
        return m_folder;
    }

    /// Marks next, the folder on the way, and keeps it in sight.
    void setNext(const QUrl &next)
    {
        m_next = next;
        markNext();
    }

    void setShowHidden(bool show)
    {
        if (m_model->dirLister()->showHiddenFiles() != show) {
            m_model->dirLister()->setShowHiddenFiles(show);
            m_model->dirLister()->emitChanges();
        }
    }

    /// The item at a row of the column.
    [[nodiscard]] KFileItem itemAt(const QModelIndex &index) const
    {
        return m_model->itemForIndex(m_proxy->mapToSource(index));
    }

protected:
    void dragMoveEvent(QDragMoveEvent *event) override
    {
        // Only folders take drops; dropping beside the items drops into the column's folder.
        const KFileItem item = itemAt(indexAt(event->position().toPoint()));
        if (item.isNull() || item.isDir()) {
            event->acceptProposedAction();
        } else {
            event->ignore();
        }
    }

    void dropEvent(QDropEvent *event) override
    {
        const KFileItem item = itemAt(indexAt(event->position().toPoint()));
        const QUrl target = item.isNull() ? m_folder : item.url();
        // The same drop the sidebar and the path segments do: KIO asks move, copy or link, and records the undo.
        KIO::DropJob *job = KIO::drop(event, target);
        KJobWidgets::setWindow(job, viewport());
        event->acceptProposedAction();
    }

    void paintEvent(QPaintEvent *event) override
    {
        QListView::paintEvent(event);
        QPainter painter(viewport());
        painter.setPen(style::HAIRLINE);
        painter.drawLine(QPointF(viewport()->width() - 0.5, 0), QPointF(viewport()->width() - 0.5, viewport()->height()));
    }

private:
    void markNext()
    {
        if (!m_next.isValid()) {
            return;
        }
        const QModelIndex source = m_model->indexForUrl(m_next);
        if (!source.isValid()) {
            return;
        }
        const QModelIndex row = m_proxy->mapFromSource(source);
        selectionModel()->setCurrentIndex(row, QItemSelectionModel::ClearAndSelect);
        scrollTo(row);
    }

    QUrl m_folder;
    QUrl m_next;
    KDirModel *m_model;
    KDirSortFilterProxyModel *m_proxy;
};

ColumnsStrip::ColumnsStrip(const QString &homePath, QWidget *parent)
    : QWidget(parent)
    , m_homePath(homePath)
    , m_scroll(new QScrollArea(this))
    , m_row(new QWidget)
    , m_layout(new QHBoxLayout(m_row))
{
    setAccessibleName(QStringLiteral("Columns"));
    auto *outer = new QHBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(m_scroll);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    // The row is as tall as the strip and as wide as its columns, wider than the strip when they do not fit.
    m_layout->setSizeConstraint(QLayout::SetMinimumSize);
    m_scroll->setWidget(m_row);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->viewport()->setAutoFillBackground(false);
    m_row->setAutoFillBackground(false);
    hide();
}

void ColumnsStrip::setFolder(const QUrl &folder)
{
    m_folder = folder;
    const QList<QUrl> wanted = columnFolders(folder, m_homePath);
    // The columns that lead to the new folder too stay as they are; the others go.
    qsizetype keep = 0;
    while (keep < m_columns.size() && keep < wanted.size() && m_columns.at(keep)->folder() == wanted.at(keep)) {
        ++keep;
    }
    while (m_columns.size() > keep) {
        // Later, not now: the click that led here may still be inside that column's mouse handler.
        FolderColumn *gone = m_columns.takeLast();
        m_layout->removeWidget(gone);
        gone->hide();
        gone->deleteLater();
    }
    for (qsizetype i = keep; i < wanted.size(); ++i) {
        auto *column = new FolderColumn(wanted.at(i), m_showHidden, m_row);
        connect(column, &QAbstractItemView::clicked, this, [this, column](const QModelIndex &index) {
            const KFileItem item = column->itemAt(index);
            if (item.isNull()) {
                return;
            }
            if (item.isDir()) {
                Q_EMIT folderChosen(item.url());
            } else {
                Q_EMIT fileChosen(item.url());
            }
        });
        m_layout->addWidget(column);
        m_columns.append(column);
    }
    for (qsizetype i = 0; i < m_columns.size(); ++i) {
        m_columns.at(i)->setNext(i + 1 < m_columns.size() ? m_columns.at(i + 1)->folder() : folder);
    }
    // No columns lead to home, to the disk or to places that are not folders: the view stands alone.
    setVisible(!m_columns.isEmpty());
    setFixedWidth(m_columns.isEmpty() ? 0 : std::min<int>(int(m_columns.size()) * COLUMN_WIDTH, 3 * COLUMN_WIDTH));
    // The innermost columns, next to the view, stay in sight.
    QTimer::singleShot(0, this, [this] { m_scroll->horizontalScrollBar()->setValue(m_scroll->horizontalScrollBar()->maximum()); });
}

void ColumnsStrip::setShowHiddenFiles(bool show)
{
    m_showHidden = show;
    for (FolderColumn *column : std::as_const(m_columns)) {
        column->setShowHidden(show);
    }
}

QList<QUrl> ColumnsStrip::folders() const
{
    QList<QUrl> urls;
    for (const FolderColumn *column : m_columns) {
        urls.append(column->folder());
    }
    return urls;
}

QListView *ColumnsStrip::column(qsizetype index) const
{
    return m_columns.value(index);
}

} // namespace fe
