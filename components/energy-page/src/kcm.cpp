// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The Info Center Energy page of Aurora (see CMakeLists.txt for why it keeps the stock plugin id).
#include "powermonitor.h"

#include <KPluginFactory>
#include <KQuickConfigModule>

#include <QQmlEngine>

// The page's module: it registers PowerMonitor for the QML and shows no extra buttons.
class KCMEnergy : public KQuickConfigModule
{
    Q_OBJECT

public:
    KCMEnergy(QObject *parent, const KPluginMetaData &data)
        : KQuickConfigModule(parent, data)
    {
        qmlRegisterType<PowerMonitor>("org.aurora.energy", 1, 0, "PowerMonitor");
        setButtons(NoAdditionalButton);
    }
};

K_PLUGIN_CLASS_WITH_JSON(KCMEnergy, "../kcm_energyinfo.json")

#include "kcm.moc"
