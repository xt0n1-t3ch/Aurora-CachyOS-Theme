// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements the RAPL arithmetic and the sysfs reading declared in power.h.

#include "power.h"

#include <QDir>
#include <QFile>

#include <limits>

namespace energy
{

double raplWatts(quint64 before, quint64 after, quint64 range, double seconds)
{
    if (seconds <= 0.0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    const quint64 microjoules = after >= before ? after - before : range - before + after;
    return static_cast<double>(microjoules) / 1e6 / seconds;
}

std::optional<quint64> readNumber(const QString &file)
{
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    bool ok = false;
    const quint64 value = f.readAll().trimmed().toULongLong(&ok);
    return ok ? std::optional(value) : std::nullopt;
}

std::optional<RaplZone> findPackageZone(const QString &powercapRoot)
{
    const QDir root(powercapRoot);
    const QStringList zones = root.entryList({QStringLiteral("intel-rapl:*")}, QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString &zone : zones) {
        QFile name(root.filePath(zone + QStringLiteral("/name")));
        if (!name.open(QIODevice::ReadOnly) || !name.readAll().trimmed().startsWith("package")) {
            continue;
        }
        const auto range = readNumber(root.filePath(zone + QStringLiteral("/max_energy_range_uj")));
        if (range) {
            return RaplZone{root.filePath(zone + QStringLiteral("/energy_uj")), *range};
        }
    }
    return std::nullopt;
}

void pushSample(QList<double> &history, double sample, qsizetype max)
{
    history.append(sample);
    if (history.size() > max) {
        history.remove(0, history.size() - max);
    }
}

} // namespace energy
