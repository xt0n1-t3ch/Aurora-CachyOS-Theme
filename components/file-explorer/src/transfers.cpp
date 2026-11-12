// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements transfers.h.

#include "transfers.h"

#include "style.h"
#include "toolbar.h"

#include <KFormat>
#include <KIO/JobTracker>

#include <QCoreApplication>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressBar>
#include <QVBoxLayout>

namespace fe {

namespace {

TransferTracker *s_tracker = nullptr;

constexpr int RING = 16;
constexpr int PANEL_WIDTH = 420;

QString sizeOf(qint64 bytes)
{
    return KFormat().formatByteSize(double(bytes));
}

QLabel *textLabel(const QString &text, qreal size, const QColor &color, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setFont(style::uiFont(size));
    QPalette colors = label->palette();
    colors.setColor(QPalette::WindowText, color);
    label->setPalette(colors);
    return label;
}

} // namespace

TransferTracker::TransferTracker(KJobTrackerInterface *inner, QObject *parent)
    : KJobTrackerInterface(parent)
    , m_inner(inner)
{
}

TransferTracker *TransferTracker::install()
{
    if (!s_tracker) {
        s_tracker = new TransferTracker(KIO::getJobTracker(), qApp);
        KIO::setJobTracker(s_tracker);
    }
    return s_tracker;
}

TransferTracker *TransferTracker::instance()
{
    return s_tracker;
}

void TransferTracker::registerJob(KJob *job)
{
    // The base class follows the job's signals into the slots below; the tracker before goes on showing it.
    KJobTrackerInterface::registerJob(job);
    if (m_inner) {
        m_inner->registerJob(job);
    }
    if (!find(job)) {
        m_entries.append(Entry{job, TransferState()});
        Q_EMIT changed();
    }
}

void TransferTracker::unregisterJob(KJob *job)
{
    KJobTrackerInterface::unregisterJob(job);
    if (m_inner) {
        m_inner->unregisterJob(job);
    }
    forget(job);
}

TransferTracker::Entry *TransferTracker::find(KJob *job)
{
    for (Entry &entry : m_entries) {
        if (entry.job == job) {
            return &entry;
        }
    }
    return nullptr;
}

void TransferTracker::forget(KJob *job)
{
    const auto before = m_entries.size();
    m_entries.removeIf([job](const Entry &entry) { return !entry.job || entry.job == job; });
    if (m_entries.size() != before) {
        Q_EMIT changed();
    }
}

QList<KJob *> TransferTracker::jobs() const
{
    QList<KJob *> found;
    for (const Entry &entry : m_entries) {
        if (entry.job) {
            found.append(entry.job);
        }
    }
    return found;
}

TransferState TransferTracker::state(KJob *job) const
{
    for (const Entry &entry : m_entries) {
        if (entry.job == job) {
            return entry.state;
        }
    }
    return {};
}

QList<TransferState> TransferTracker::states() const
{
    QList<TransferState> found;
    for (const Entry &entry : m_entries) {
        if (entry.job) {
            found.append(entry.state);
        }
    }
    return found;
}

void TransferTracker::finished(KJob *job)
{
    forget(job);
}

void TransferTracker::suspended(KJob *job)
{
    if (Entry *entry = find(job)) {
        entry->state.suspended = true;
        Q_EMIT changed();
    }
}

void TransferTracker::resumed(KJob *job)
{
    if (Entry *entry = find(job)) {
        entry->state.suspended = false;
        Q_EMIT changed();
    }
}

void TransferTracker::description(KJob *job, const QString &title, const QPair<QString, QString> &field1, const QPair<QString, QString> &field2)
{
    if (Entry *entry = find(job)) {
        entry->state.title = title;
        entry->state.source = field1.second;
        entry->state.destination = field2.second;
        Q_EMIT changed();
    }
}

void TransferTracker::totalAmount(KJob *job, KJob::Unit unit, qulonglong amount)
{
    if (Entry *entry = find(job)) {
        if (unit == KJob::Bytes) {
            entry->state.totalBytes = qint64(amount);
        } else if (unit == KJob::Files) {
            entry->state.totalFiles = qint64(amount);
        }
        Q_EMIT changed();
    }
}

void TransferTracker::processedAmount(KJob *job, KJob::Unit unit, qulonglong amount)
{
    if (Entry *entry = find(job)) {
        if (unit == KJob::Bytes) {
            entry->state.processedBytes = qint64(amount);
        } else if (unit == KJob::Files) {
            entry->state.processedFiles = qint64(amount);
        }
        Q_EMIT changed();
    }
}

void TransferTracker::percent(KJob *job, unsigned long value)
{
    if (Entry *entry = find(job)) {
        entry->state.percent = int(value);
        Q_EMIT changed();
    }
}

void TransferTracker::speed(KJob *job, unsigned long value)
{
    if (Entry *entry = find(job)) {
        entry->state.speed = qint64(value);
        Q_EMIT changed();
    }
}

TransferButton::TransferButton(TransferTracker *tracker, QWidget *parent)
    : QWidget(parent)
    , m_tracker(tracker)
{
    setFont(style::uiFont(12));
    setCursor(Qt::PointingHandCursor);
    setAccessibleName(QStringLiteral("Transfers"));
    connect(tracker, &TransferTracker::changed, this, &TransferButton::refresh);
    refresh();
}

void TransferButton::refresh()
{
    const QList<TransferState> states = m_tracker->states();
    m_label = transfersLabel(states);
    m_percent = combinedPercent(states);
    setAccessibleDescription(m_label);
    setVisible(!states.isEmpty());
    updateGeometry();
    update();
}

QString TransferButton::label() const
{
    return m_label;
}

QSize TransferButton::sizeHint() const
{
    return {RING + 18 + fontMetrics().horizontalAdvance(m_label) + 14, style::CAPSULE_HEIGHT - 6};
}

void TransferButton::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF bounds = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    style::paintGlass(painter, bounds, bounds.height() / 2.0, underMouse() ? style::CAPSULE_HOVER : style::CAPSULE_FILL);
    // A ring that fills clockwise from the top, as Finder's progress in its toolbar.
    const QRectF ring(10, (height() - RING) / 2.0, RING, RING);
    painter.setPen(QPen(style::CAPACITY_TRACK, 2.5));
    painter.drawEllipse(ring);
    painter.setPen(QPen(style::ACCENT, 2.5, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(ring, 90 * 16, -int(m_percent * 360 * 16 / 100));
    painter.setPen(style::TEXT);
    painter.drawText(QRectF(ring.right() + 8, 0, width(), height()), Qt::AlignVCenter | Qt::AlignLeft, m_label);
}

void TransferButton::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        return;
    }
    auto *panel = new TransferPanel(m_tracker, window());
    panel->adjustSize();
    const QPoint below = mapToGlobal(QPoint(width() - panel->width(), height() + 6));
    panel->move(below);
    panel->show();
}

TransferPanel::TransferPanel(TransferTracker *tracker, QWidget *parent)
    : QWidget(parent, Qt::Popup)
    , m_tracker(tracker)
    , m_rows(new QVBoxLayout(this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    setAccessibleName(QStringLiteral("Transfers"));
    setFixedWidth(PANEL_WIDTH);
    m_rows->setContentsMargins(14, 12, 14, 12);
    m_rows->setSpacing(10);
    connect(tracker, &TransferTracker::changed, this, &TransferPanel::rebuild);
    rebuild();
}

void TransferPanel::rebuild()
{
    const QList<KJob *> jobs = m_tracker->jobs();
    if (jobs.isEmpty()) {
        close();
        return;
    }
    while (QLayoutItem *item = m_rows->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    for (KJob *job : jobs) {
        const TransferState state = m_tracker->state(job);
        auto *row = new QWidget(this);
        auto *layout = new QVBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(3);
        auto *head = new QHBoxLayout;
        head->addWidget(textLabel(state.title.isEmpty() ? QStringLiteral("Transfer") : state.title, 13, style::TEXT, row), 1);
        const QPointer<KJob> guard(job);
        auto *pause = new GlassButton(state.suspended ? QStringLiteral("media-playback-start-symbolic") : QStringLiteral("media-playback-pause-symbolic"),
                                      state.suspended ? QStringLiteral("Resume") : QStringLiteral("Pause"), row);
        pause->setStandalone(true);
        pause->setEnabled(job->capabilities() & KJob::Suspendable);
        connect(pause, &QAbstractButton::clicked, this, [guard] {
            if (guard) {
                guard->isSuspended() ? guard->resume() : guard->suspend();
            }
        });
        auto *stop = new GlassButton(QStringLiteral("process-stop-symbolic"), QStringLiteral("Stop"), row);
        stop->setStandalone(true);
        stop->setEnabled(job->capabilities() & KJob::Killable);
        // EmitResult, so the job reports it ended and KIO's undo records what was done before the stop.
        connect(stop, &QAbstractButton::clicked, this, [guard] {
            if (guard) {
                guard->kill(KJob::EmitResult);
            }
        });
        head->addWidget(pause);
        head->addWidget(stop);
        layout->addLayout(head);
        const QString route = state.destination.isEmpty() ? state.source : QStringLiteral("%1 to %2").arg(state.source, state.destination);
        if (!route.isEmpty()) {
            QLabel *where = textLabel(route, 11, style::TEXT_SECONDARY, row);
            where->setText(where->fontMetrics().elidedText(route, Qt::ElideMiddle, PANEL_WIDTH - 40));
            where->setToolTip(route);
            layout->addWidget(where);
        }
        auto *bar = new QProgressBar(row);
        bar->setRange(0, 100);
        bar->setValue(combinedPercent({state}));
        bar->setTextVisible(false);
        bar->setFixedHeight(6);
        layout->addWidget(bar);
        layout->addWidget(textLabel(transferProgressText(state, sizeOf), 11, style::TEXT_SECONDARY, row));
        m_rows->addWidget(row);
    }
    adjustSize();
}

void TransferPanel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    style::paintGlass(painter, QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), style::MENU_RADIUS, style::MENU_FILL);
}

} // namespace fe
