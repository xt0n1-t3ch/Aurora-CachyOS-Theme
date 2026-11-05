// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The object the Energy page's QML reads: sampled power, temperatures, history and the power profile.

#pragma once

#include "power.h"

#include <QDBusVariant>
#include <QElapsedTimer>
#include <QObject>
#include <QStringList>
#include <QTimer>

#include <memory>

class Nvml;

// Live power readings for the Energy page, sampled once a second while active: the CPU package from RAPL, the
// first NVIDIA GPU from NVML (loaded at runtime, so the page works without the driver), both temperatures, and the
// power profile from power-profiles-daemon's D-Bus API (tuned-ppd serves it on CachyOS). Nothing runs while inactive.
// Every reading property changes together with `sampled`.
class PowerMonitor : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)
    Q_PROPERTY(bool cpuAvailable READ cpuAvailable NOTIFY sampled)
    Q_PROPERTY(bool gpuAvailable READ gpuAvailable NOTIFY sampled)
    Q_PROPERTY(double cpuWatts READ cpuWatts NOTIFY sampled)
    Q_PROPERTY(double gpuWatts READ gpuWatts NOTIFY sampled)
    Q_PROPERTY(double cpuTemperature READ cpuTemperature NOTIFY sampled)
    Q_PROPERTY(double gpuTemperature READ gpuTemperature NOTIFY sampled)
    Q_PROPERTY(QString gpuName READ gpuName NOTIFY sampled)
    Q_PROPERTY(double sessionWattHours READ sessionWattHours NOTIFY sampled)
    Q_PROPERTY(QList<double> cpuHistory READ cpuHistory NOTIFY sampled)
    Q_PROPERTY(QList<double> gpuHistory READ gpuHistory NOTIFY sampled)
    Q_PROPERTY(int historyLength READ historyLength CONSTANT)
    Q_PROPERTY(QString profile READ profile WRITE setProfile NOTIFY profileChanged)
    Q_PROPERTY(QStringList profiles READ profiles NOTIFY profileChanged)

public:
    explicit PowerMonitor(QObject *parent = nullptr);
    ~PowerMonitor() override;

    // Whether the one-second sampling runs. Turning it on loads NVML if needed and samples at once.
    bool active() const { return m_timer.isActive(); }
    void setActive(bool active);
    // Whether the RAPL counter is readable.
    bool cpuAvailable() const { return m_zone.has_value(); }
    // Whether NVML answered at the last sample.
    bool gpuAvailable() const { return m_gpuAvailable; }
    // Watts since the previous sample; 0 until a reading exists.
    double cpuWatts() const { return m_cpuWatts; }
    double gpuWatts() const { return m_gpuWatts; }
    // Degrees Celsius; 0 until a reading exists.
    double cpuTemperature() const { return m_cpuTemperature; }
    double gpuTemperature() const { return m_gpuTemperature; }
    QString gpuName() const { return m_gpuName; }
    // Energy used by the CPU and the GPU together since this object was created, in watt hours.
    double sessionWattHours() const { return m_joules / 3600.0; }
    // The last historyLength() watt readings, oldest first.
    QList<double> cpuHistory() const { return m_cpuHistory; }
    QList<double> gpuHistory() const { return m_gpuHistory; }
    int historyLength() const { return HISTORY; }
    // The active power profile (for example "balanced"); empty without the daemon.
    QString profile() const { return m_profile; }
    // Asks the daemon to switch. The property changes when the daemon reports it, not here; unknown names are ignored.
    void setProfile(const QString &profile);
    // The profile names the daemon offers.
    QStringList profiles() const { return m_profiles; }

Q_SIGNALS:
    void activeChanged();
    // Emitted after every sample, when all the readings are up to date.
    void sampled();
    // Emitted when the active profile or the list of profiles changed.
    void profileChanged();

private Q_SLOTS:
    void onProfilePropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &invalidated);

private:
    static constexpr int HISTORY = 120; // Two minutes at one sample a second.

    void sample();
    void readProfiles();

    QTimer m_timer;
    QElapsedTimer m_uptime; // Monotonic time for m_samples, started with the monitor.
    energy::SampleClock m_samples;
    std::optional<energy::RaplZone> m_zone;
    std::optional<quint64> m_lastCounter;
    QString m_cpuTemperatureFile;
    std::unique_ptr<Nvml> m_nvml;
    bool m_gpuAvailable = false;
    double m_cpuWatts = 0;
    double m_gpuWatts = 0;
    double m_cpuTemperature = 0;
    double m_gpuTemperature = 0;
    double m_joules = 0;
    QString m_gpuName;
    QList<double> m_cpuHistory;
    QList<double> m_gpuHistory;
    QString m_profile;
    QStringList m_profiles;
};
