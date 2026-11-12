// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The transfer center: every copy, move and deletion KIO runs in this app, shown in the window's action row and in a
// panel where each can be paused, resumed or stopped. The texts it shows come from core/transfers.

#pragma once

#include "core/transfers.h"

#include <KJobTrackerInterface>

#include <QPointer>
#include <QWidget>

class QVBoxLayout;

namespace fe {

/// A job tracker set in front of KIO's own (KIO::setJobTracker): each job goes on to the tracker that was there before,
/// so Plasma's notifications still show it, and is also kept here for the window. Lives as long as the app.
class TransferTracker : public KJobTrackerInterface
{
    Q_OBJECT

public:
    /// inner is the tracker jobs go on to; null for none (the tests).
    explicit TransferTracker(KJobTrackerInterface *inner, QObject *parent = nullptr);

    /// Puts a tracker in front of KIO's and returns it; the same one on every call.
    static TransferTracker *install();
    /// The tracker install() set, or null before.
    static TransferTracker *instance();

    void registerJob(KJob *job) override;
    void unregisterJob(KJob *job) override;

    /// The running jobs, oldest first.
    [[nodiscard]] QList<KJob *> jobs() const;
    [[nodiscard]] TransferState state(KJob *job) const;
    [[nodiscard]] QList<TransferState> states() const;

Q_SIGNALS:
    /// A job came, went, or moved on.
    void changed();

protected Q_SLOTS:
    void finished(KJob *job) override;
    void suspended(KJob *job) override;
    void resumed(KJob *job) override;
    void description(KJob *job, const QString &title, const QPair<QString, QString> &field1, const QPair<QString, QString> &field2) override;
    void totalAmount(KJob *job, KJob::Unit unit, qulonglong amount) override;
    void processedAmount(KJob *job, KJob::Unit unit, qulonglong amount) override;
    void percent(KJob *job, unsigned long percent) override;
    void speed(KJob *job, unsigned long value) override;

private:
    struct Entry {
        QPointer<KJob> job;
        TransferState state;
    };
    Entry *find(KJob *job);
    void forget(KJob *job);

    QPointer<KJobTrackerInterface> m_inner;
    QList<Entry> m_entries;
};

/// The transfer center's button in the action row: a ring that fills as the transfers go and their short label.
/// Hidden while nothing runs; a click opens the panel.
class TransferButton : public QWidget
{
    Q_OBJECT

public:
    explicit TransferButton(TransferTracker *tracker, QWidget *parent = nullptr);
    [[nodiscard]] QSize sizeHint() const override;
    /// The label as shown.
    [[nodiscard]] QString label() const;

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void refresh();

    TransferTracker *m_tracker;
    QString m_label;
    int m_percent = 0;
};

/// The panel of the transfer center: a row per job with its title, where from and to, a bar, its progress line, and
/// buttons to pause or resume it and to stop it. Closes when the last job ends or on a click elsewhere.
class TransferPanel : public QWidget
{
    Q_OBJECT

public:
    TransferPanel(TransferTracker *tracker, QWidget *parent);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void rebuild();

    TransferTracker *m_tracker;
    QVBoxLayout *m_rows;
};

} // namespace fe
