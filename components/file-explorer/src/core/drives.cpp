// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements drives.h.

#include "core/drives.h"

#include <QCollator>

#include <algorithm>
#include <numeric>

namespace fe {

namespace {

// The sidebar group of a kind: system disk, built-in drives, removable drives, then network drives.
int groupOf(DriveKind kind)
{
    switch (kind) {
    case DriveKind::System:
        return 0;
    case DriveKind::Internal:
        return 1;
    case DriveKind::Network:
        return 3;
    default:
        return 2;
    }
}

} // namespace

QString systemDriveName(const QString &hostName)
{
    QString name = hostName;
    name.replace(u'-', u' ').replace(u'_', u' ');
    name = name.simplified();
    return name.isEmpty() ? QStringLiteral("System") : name;
}

QString driveName(const Drive &drive, const QMap<QString, QString> &names, const QString &hostName)
{
    const QString own = names.value(drive.key).trimmed();
    if (!own.isEmpty()) {
        return own;
    }
    if (drive.kind == DriveKind::System) {
        return systemDriveName(hostName);
    }
    return drive.label;
}

QString cleanDriveName(const QString &typed)
{
    return typed.simplified().left(64).trimmed();
}

QList<qsizetype> driveOrder(const QList<Drive> &drives, const QMap<QString, QString> &names, const QString &hostName)
{
    QList<qsizetype> order(drives.size());
    std::iota(order.begin(), order.end(), 0);
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    QList<QString> shown;
    for (const Drive &drive : drives) {
        shown.append(driveName(drive, names, hostName));
    }
    std::stable_sort(order.begin(), order.end(), [&](qsizetype a, qsizetype b) {
        const int groupA = groupOf(drives.at(a).kind);
        const int groupB = groupOf(drives.at(b).kind);
        if (groupA != groupB) {
            return groupA < groupB;
        }
        return collator.compare(shown.at(a), shown.at(b)) < 0;
    });
    return order;
}

Capacity capacityFor(quint64 size, quint64 available)
{
    Capacity capacity;
    if (size == 0 || available >= size) {
        return capacity;
    }
    capacity.used = qreal(size - available) / qreal(size);
    if (capacity.used > 0.95) {
        capacity.level = Capacity::Level::Critical;
    } else if (capacity.used > 0.90) {
        capacity.level = Capacity::Level::Low;
    }
    return capacity;
}

QString capacityText(quint64 size, quint64 available, const std::function<QString(qint64)> &formatSize)
{
    return QStringLiteral("%1 free of %2").arg(formatSize(qint64(std::min(available, size))), formatSize(qint64(size)));
}

QString driveSymbol(DriveKind kind)
{
    switch (kind) {
    case DriveKind::System:
        return QStringLiteral(":/icons/places/drive-system.svg");
    case DriveKind::Internal:
        return QStringLiteral(":/icons/places/drive-internal.svg");
    case DriveKind::External:
        return QStringLiteral(":/icons/places/drive-external.svg");
    case DriveKind::Stick:
        return QStringLiteral(":/icons/places/drive-stick.svg");
    case DriveKind::Optical:
        return QStringLiteral(":/icons/places/drive-optical.svg");
    case DriveKind::Card:
        return QStringLiteral(":/icons/places/drive-card.svg");
    case DriveKind::Network:
        return QStringLiteral(":/icons/places/drive-network.svg");
    }
    return {};
}

} // namespace fe
