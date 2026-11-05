// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Samples RAPL, NVML and hwmon once a second and follows the power profile over D-Bus.

#include "powermonitor.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDir>
#include <QFile>

#include <dlfcn.h>

#include <cmath>

namespace {

// The service name is also the name of the interface that holds the profile properties.
const QString PROFILES_SERVICE = QStringLiteral("org.freedesktop.UPower.PowerProfiles");
const QString PROFILES_PATH = QStringLiteral("/org/freedesktop/UPower/PowerProfiles");
const QString PROPERTIES = QStringLiteral("org.freedesktop.DBus.Properties");

// Path of temp1_input of the CPU's hwmon sensor: k10temp (AMD, Tctl) or coretemp (Intel, package). Empty when
// neither chip exists.
QString cpuTemperatureFile()
{
    const QDir hwmon(QStringLiteral("/sys/class/hwmon"));
    for (const QString &entry : hwmon.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        QFile name(hwmon.filePath(entry + QStringLiteral("/name")));
        if (name.open(QIODevice::ReadOnly)) {
            const QByteArray chip = name.readAll().trimmed();
            if (chip == "k10temp" || chip == "coretemp") {
                return hwmon.filePath(entry + QStringLiteral("/temp1_input"));
            }
        }
    }
    return {};
}

} // namespace

// The few NVML calls the page needs, resolved from the driver's libnvidia-ml.so.1 at runtime (NVML's C API,
// nvml.h of driver 615: every call returns 0 on success). It reads the first GPU only, and ready() is false when
// the library, a symbol or the device is missing, in which case every reading is empty.
class Nvml
{
public:
    Nvml()
    {
        m_library = dlopen("libnvidia-ml.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!m_library) {
            return;
        }
        m_init = reinterpret_cast<int (*)()>(dlsym(m_library, "nvmlInit_v2"));
        m_shutdown = reinterpret_cast<int (*)()>(dlsym(m_library, "nvmlShutdown"));
        m_handle = reinterpret_cast<int (*)(unsigned, void **)>(dlsym(m_library, "nvmlDeviceGetHandleByIndex_v2"));
        m_power = reinterpret_cast<int (*)(void *, unsigned *)>(dlsym(m_library, "nvmlDeviceGetPowerUsage"));
        m_temperature = reinterpret_cast<int (*)(void *, int, unsigned *)>(dlsym(m_library, "nvmlDeviceGetTemperature"));
        m_name = reinterpret_cast<int (*)(void *, char *, unsigned)>(dlsym(m_library, "nvmlDeviceGetName"));
        if (m_init && m_shutdown && m_handle && m_power && m_temperature && m_name && m_init() == 0) {
            m_ready = m_handle(0, &m_device) == 0;
        }
    }

    ~Nvml()
    {
        if (m_ready) {
            m_shutdown();
        }
        if (m_library) {
            dlclose(m_library);
        }
    }

    bool ready() const { return m_ready; }

    // Power draw in watts; NVML reports milliwatts.
    std::optional<double> watts() const
    {
        unsigned milliwatts = 0;
        return m_ready && m_power(m_device, &milliwatts) == 0 ? std::optional(milliwatts / 1000.0) : std::nullopt;
    }

    std::optional<double> celsius() const
    {
        unsigned degrees = 0;
        // NVML_TEMPERATURE_GPU is 0.
        return m_ready && m_temperature(m_device, 0, &degrees) == 0 ? std::optional(double(degrees)) : std::nullopt;
    }

    QString name() const
    {
        char buffer[96] = {};
        return m_ready && m_name(m_device, buffer, sizeof buffer) == 0 ? QString::fromUtf8(buffer) : QString();
    }

private:
    void *m_library = nullptr;
    void *m_device = nullptr;
    bool m_ready = false;
    int (*m_init)() = nullptr;
    int (*m_shutdown)() = nullptr;
    int (*m_handle)(unsigned, void **) = nullptr;
    int (*m_power)(void *, unsigned *) = nullptr;
    int (*m_temperature)(void *, int, unsigned *) = nullptr;
    int (*m_name)(void *, char *, unsigned) = nullptr;
};

PowerMonitor::PowerMonitor(QObject *parent)
    : QObject(parent)
    , m_zone(energy::findPackageZone(QStringLiteral("/sys/class/powercap")))
    , m_cpuTemperatureFile(cpuTemperatureFile())
{
    m_uptime.start();
    m_timer.setInterval(1000);
    // Follows profile changes made elsewhere, such as by System Settings.
    connect(&m_timer, &QTimer::timeout, this, &PowerMonitor::sample);
    QDBusConnection::systemBus().connect(PROFILES_SERVICE, PROFILES_PATH, PROPERTIES, QStringLiteral("PropertiesChanged"), this,
                                         SLOT(onProfilePropertiesChanged(QString, QVariantMap, QStringList)));
    readProfiles();
}

PowerMonitor::~PowerMonitor() = default;

void PowerMonitor::setActive(bool active)
{
    if (active == m_timer.isActive()) {
        return;
    }
    if (active) {
        // NVML is loaded only for a page on screen, and stays loaded until the page goes away.
        if (!m_nvml) {
            m_nvml = std::make_unique<Nvml>();
            m_gpuName = m_nvml->name();
        }
        // A reopened page starts a new run of samples: neither the counter nor the time of the last run counts.
        m_lastCounter.reset();
        m_samples.restart();
        sample();
        m_timer.start();
    } else {
        m_timer.stop();
    }
    Q_EMIT activeChanged();
}

void PowerMonitor::sample()
{
    const double seconds = m_samples.next(m_uptime.nsecsElapsed());
    // A watt reading needs two counter values, so the first sample after activation only stores one.
    if (m_zone) {
        const auto counter = energy::readNumber(m_zone->counter);
        if (counter && m_lastCounter) {
            const double watts = energy::raplWatts(*m_lastCounter, *counter, m_zone->range, seconds);
            if (!std::isnan(watts)) {
                m_cpuWatts = watts;
            }
        }
        if (!counter) {
            m_zone.reset(); // not readable: the page says so instead of showing zero
        }
        m_lastCounter = counter;
    }
    // The hwmon file holds millidegrees Celsius.
    if (const auto milli = energy::readNumber(m_cpuTemperatureFile)) {
        m_cpuTemperature = *milli / 1000.0;
    }
    const auto gpu = m_nvml ? m_nvml->watts() : std::nullopt;
    m_gpuAvailable = gpu.has_value();
    if (gpu) {
        m_gpuWatts = *gpu;
        m_gpuTemperature = m_nvml->celsius().value_or(m_gpuTemperature);
    }
    if (seconds > 0) {
        m_joules += (m_cpuWatts + m_gpuWatts) * seconds;
        energy::pushSample(m_cpuHistory, m_cpuWatts, HISTORY);
        energy::pushSample(m_gpuHistory, m_gpuWatts, HISTORY);
    }
    Q_EMIT sampled();
}

// Reads the active profile and the profile list with a blocking call (2 seconds at most). Leaves both empty when
// the daemon does not answer.
void PowerMonitor::readProfiles()
{
    QDBusMessage get = QDBusMessage::createMethodCall(PROFILES_SERVICE, PROFILES_PATH, PROPERTIES, QStringLiteral("GetAll"));
    get << PROFILES_SERVICE;
    const QDBusMessage reply = QDBusConnection::systemBus().call(get, QDBus::Block, 2000);
    if (reply.type() != QDBusMessage::ReplyMessage) {
        return;
    }
    const QVariantMap properties = qdbus_cast<QVariantMap>(reply.arguments().constFirst());
    m_profile = properties.value(QStringLiteral("ActiveProfile")).toString();
    m_profiles.clear();
    const auto list = properties.value(QStringLiteral("Profiles")).value<QDBusArgument>();
    list.beginArray();
    while (!list.atEnd()) {
        QVariantMap entry;
        list >> entry;
        m_profiles.append(entry.value(QStringLiteral("Profile")).toString());
    }
    list.endArray();
    Q_EMIT profileChanged();
}

void PowerMonitor::setProfile(const QString &profile)
{
    if (profile == m_profile || !m_profiles.contains(profile)) {
        return;
    }
    QDBusMessage set = QDBusMessage::createMethodCall(PROFILES_SERVICE, PROFILES_PATH, PROPERTIES, QStringLiteral("Set"));
    set << PROFILES_SERVICE << QStringLiteral("ActiveProfile") << QVariant::fromValue(QDBusVariant(profile));
    QDBusConnection::systemBus().asyncCall(set);
}

void PowerMonitor::onProfilePropertiesChanged(const QString &interface, const QVariantMap &changed, const QStringList &)
{
    if (interface == PROFILES_SERVICE && changed.contains(QStringLiteral("ActiveProfile"))) {
        m_profile = changed.value(QStringLiteral("ActiveProfile")).toString();
        Q_EMIT profileChanged();
    }
}
