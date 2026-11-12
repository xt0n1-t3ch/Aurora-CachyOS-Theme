// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// What the transfer center says about running copies, moves and deletions: how far each is, all of them together, and
// the time left.

#pragma once

#include <QList>
#include <QString>

#include <functional>

namespace fe {

/// One running transfer as KIO reports it.
struct TransferState {
    /// KIO's own title: "Copying", "Moving", "Deleting".
    QString title;
    /// Where from and where to, as KIO describes them; either may be empty.
    QString source;
    QString destination;
    qint64 processedBytes = 0;
    qint64 totalBytes = 0;
    qint64 processedFiles = 0;
    qint64 totalFiles = 0;
    /// Bytes per second.
    qint64 speed = 0;
    /// KIO's percent, used when no byte totals are known.
    int percent = 0;
    bool suspended = false;
};

/// The share done of all transfers together, 0 to 100: by bytes where the totals are known, else KIO's percents.
[[nodiscard]] int combinedPercent(const QList<TransferState> &transfers);

/// The time left at speed bytes per second for remaining bytes, in plain words: "About 2 minutes left", "A few
/// seconds left". Empty when it cannot be told (no speed, nothing left).
[[nodiscard]] QString timeLeftText(qint64 remainingBytes, qint64 speed);

/// A transfer's progress line: "45 MB of 1.2 GB, 12 MB/s, About 2 minutes left", "3 of 10 files", or "Paused".
[[nodiscard]] QString transferProgressText(const TransferState &transfer, const std::function<QString(qint64)> &formatSize);

/// The window's short label for all transfers: "Copying, 45%" for one, "3 transfers, 45%" for several.
[[nodiscard]] QString transfersLabel(const QList<TransferState> &transfers);

} // namespace fe
