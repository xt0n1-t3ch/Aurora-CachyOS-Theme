// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The Energy page's measurements as plain functions: RAPL watts, sysfs reading and the sample history.

#pragma once

#include <QList>
#include <QString>

#include <optional>

// What the Energy page measures, kept free of Qt Quick so it can be tested on fake sysfs trees.
namespace energy
{

// Watts drawn between two readings of a RAPL energy counter in microjoules. The counter wraps to zero at `range`
// (max_energy_range_uj), so a smaller second reading means it wrapped once. NaN when `seconds` is zero or less.
double raplWatts(quint64 before, quint64 after, quint64 range, double seconds);

struct RaplZone {
    QString counter; // Path of the energy_uj file.
    quint64 range = 0; // The value max_energy_range_uj, in microjoules.
};

// The CPU package zone of a powercap tree (/sys/class/powercap): the first intel-rapl zone, by directory name, whose
// name starts with "package" and whose range is readable. Nothing when there is none.
std::optional<RaplZone> findPackageZone(const QString &powercapRoot);

// A non-negative integer from a sysfs file; nothing when the file is missing, unreadable or not a number.
std::optional<quint64> readNumber(const QString &file);

// Appends a sample and drops the oldest ones beyond `max`.
void pushSample(QList<double> &history, double sample, qsizetype max);

// The seconds each sample covers. The first sample after restart() covers none, so the time the page was closed adds
// nothing to the energy total or the chart.
class SampleClock
{
public:
    // Forgets the previous sample; call it when sampling starts again.
    void restart()
    {
        m_last.reset();
    }

    // Seconds since the previous call, given a monotonic time in nanoseconds; 0 for the first call after restart().
    double next(qint64 nowNs)
    {
        const double seconds = m_last ? static_cast<double>(nowNs - *m_last) / 1e9 : 0.0;
        m_last = nowNs;
        return seconds;
    }

private:
    std::optional<qint64> m_last;
};

} // namespace energy
