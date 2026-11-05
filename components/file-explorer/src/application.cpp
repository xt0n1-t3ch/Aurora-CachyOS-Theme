// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Application-wide setup: names the application, installs the Finder style and the dark palette, and builds the path
// context.

#include "application.h"

#include "finderstyle.h"
#include "style.h"

#include "core/drives.h"
#include "core/motion.h"

#include <KConfigGroup>
#include <KConfigWatcher>
#include <KSharedConfig>

#include <QApplication>
#include <QDir>
#include <QIcon>
#include <QStyleFactory>
#include <QSysInfo>

namespace fe {

void configureApplication()
{
    QApplication::setApplicationName(QStringLiteral("file-explorer"));
    QApplication::setApplicationDisplayName(QStringLiteral("File Explorer"));
    QApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QApplication::setDesktopFileName(QStringLiteral("file-explorer"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("file-explorer-mac"), QIcon::fromTheme(QStringLiteral("system-file-manager"))));

    QStyle *base = QStyleFactory::create(QStringLiteral("Breeze"));
    if (!base) {
        base = QStyleFactory::create(QStringLiteral("Fusion"));
    }
    // Transitions follow the desktop's animation speed (core/motion.h), and pick up a new one while the app runs.
    const KSharedConfigPtr globals = KSharedConfig::openConfig(QStringLiteral("kdeglobals"));
    motion::setSpeed(globals->group(QStringLiteral("KDE")).readEntry("AnimationDurationFactor", 1.0));
    static const KConfigWatcher::Ptr speedWatcher = KConfigWatcher::create(globals);
    QObject::connect(speedWatcher.data(), &KConfigWatcher::configChanged, qApp, [](const KConfigGroup &group, const QByteArrayList &names) {
        if (group.name() == QLatin1String("KDE") && names.contains(QByteArrayLiteral("AnimationDurationFactor"))) {
            motion::setSpeed(group.readEntry("AnimationDurationFactor", 1.0));
        }
    });

    QApplication::setStyle(new FinderStyle(base));
    QApplication::setPalette(style::darkPalette());
    QApplication::setFont(style::uiFont(13));
}

PathContext currentPathContext()
{
    // macOS calls the system disk "Macintosh HD"; here it carries the name the user gave it in the sidebar, or else
    // the computer's host name, which is also the name its sidebar row shows.
    const QString user = qEnvironmentVariable("USER", QDir::home().dirName());
    const QMap<QString, QString> names = KSharedConfig::openConfig(QStringLiteral("file-explorerrc"))->group(QStringLiteral("Drive Names")).entryMap();
    const Drive system{QStringLiteral("system"), QString(), DriveKind::System};
    return {QDir::homePath(), user, driveName(system, names, QSysInfo::machineHostName())};
}

} // namespace fe
