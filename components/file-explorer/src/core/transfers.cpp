// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements transfers.h.

#include "core/transfers.h"

#include <QStringList>

namespace fe {

int combinedPercent(const QList<TransferState> &transfers)
{
    qint64 done = 0;
    qint64 total = 0;
    bool allSized = !transfers.isEmpty();
    for (const TransferState &transfer : transfers) {
        allSized = allSized && transfer.totalBytes > 0;
        done += transfer.processedBytes;
        total += transfer.totalBytes;
    }
    if (allSized && total > 0) {
        return int(std::clamp<qint64>(done * 100 / total, 0, 100));
    }
    if (transfers.isEmpty()) {
        return 0;
    }
    int sum = 0;
    for (const TransferState &transfer : transfers) {
        sum += std::clamp(transfer.percent, 0, 100);
    }
    return sum / int(transfers.size());
}

QString timeLeftText(qint64 remainingBytes, qint64 speed)
{
    if (speed <= 0 || remainingBytes <= 0) {
        return {};
    }
    const qint64 seconds = (remainingBytes + speed - 1) / speed;
    if (seconds < 10) {
        return QStringLiteral("A few seconds left");
    }
    if (seconds < 60) {
        return QStringLiteral("About %1 seconds left").arg((seconds + 5) / 10 * 10);
    }
    const qint64 minutes = (seconds + 30) / 60;
    if (minutes < 60) {
        return minutes == 1 ? QStringLiteral("About a minute left") : QStringLiteral("About %1 minutes left").arg(minutes);
    }
    const qint64 hours = (minutes + 30) / 60;
    return hours == 1 ? QStringLiteral("About an hour left") : QStringLiteral("About %1 hours left").arg(hours);
}

QString transferProgressText(const TransferState &transfer, const std::function<QString(qint64)> &formatSize)
{
    if (transfer.suspended) {
        return QStringLiteral("Paused");
    }
    QStringList parts;
    if (transfer.totalBytes > 0) {
        parts.append(QStringLiteral("%1 of %2").arg(formatSize(transfer.processedBytes), formatSize(transfer.totalBytes)));
    } else if (transfer.totalFiles > 0) {
        parts.append(QStringLiteral("%1 of %2 files").arg(transfer.processedFiles).arg(transfer.totalFiles));
    }
    if (transfer.speed > 0) {
        parts.append(formatSize(transfer.speed) + QStringLiteral("/s"));
    }
    if (const QString left = timeLeftText(transfer.totalBytes - transfer.processedBytes, transfer.speed); !left.isEmpty()) {
        parts.append(left);
    }
    return parts.isEmpty() ? QStringLiteral("Starting") : parts.join(QStringLiteral(", "));
}

QString transfersLabel(const QList<TransferState> &transfers)
{
    if (transfers.isEmpty()) {
        return {};
    }
    const QString what = transfers.size() == 1 ? (transfers.constFirst().title.isEmpty() ? QStringLiteral("Transfer") : transfers.constFirst().title)
                                               : QStringLiteral("%1 transfers").arg(transfers.size());
    return QStringLiteral("%1, %2%").arg(what).arg(combinedPercent(transfers));
}

} // namespace fe
