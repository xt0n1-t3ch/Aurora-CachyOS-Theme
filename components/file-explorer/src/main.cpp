// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Entry point: opens one window per folder argument, or runs as the D-Bus FileManager1 service or the FileChooser
// portal backend.

#include "application.h"
#include "filemanager1.h"
#include "portal.h"
#include "mainwindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QDBusConnection>
#include <QDir>
#include <QFileInfo>
#include <QTimer>
#include <QWidget>

#include <algorithm>

namespace {

/// The folder to show for a command-line argument; a file opens its folder.
QUrl folderFor(const QString &argument)
{
    const QUrl url = QUrl::fromUserInput(argument, QDir::currentPath(), QUrl::AssumeLocalFile);
    if (url.isLocalFile()) {
        const QFileInfo info(url.toLocalFile());
        if (info.exists() && !info.isDir()) {
            return QUrl::fromLocalFile(info.absolutePath());
        }
    }
    return url;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    fe::configureApplication();

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("A Finder-style file manager built on Dolphin's view."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument(QStringLiteral("urls"), QStringLiteral("Folders to open, one window each."), QStringLiteral("[urls...]"));
    const QCommandLineOption service(QStringLiteral("service"),
                                     QStringLiteral("Serve org.freedesktop.FileManager1 on the session bus (D-Bus activation)."));
    parser.addOption(service);
    const QCommandLineOption portal(QStringLiteral("portal"),
                                    QStringLiteral("Serve the FileChooser portal backend (Open and Save panels) on the session bus."));
    parser.addOption(portal);
    parser.process(app);

    const fe::PathContext context = fe::currentPathContext();
    if (parser.isSet(portal)) {
        // xdg-desktop-portal asks this backend for every Open and Save panel; it stays up between panels.
        auto *host = new QObject(&app);
        new FileChooserPortal(context, host);
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.registerObject(QStringLiteral("/org/freedesktop/portal/desktop"), host)
            || !bus.registerService(QStringLiteral("org.freedesktop.impl.portal.desktop.aurora"))) {
            return 1;
        }
        // Neither the last panel closing nor KIO releasing its event loop lockers may end the backend.
        QApplication::setQuitOnLastWindowClosed(false);
        QCoreApplication::setQuitLockEnabled(false);
        // Three minutes without a panel on screen and the backend leaves; D-Bus starts it again for the next panel.
        QTimer idle;
        idle.setInterval(180000);
        QObject::connect(&idle, &QTimer::timeout, &app, [] {
            const QWidgetList windows = QApplication::topLevelWidgets();
            if (std::none_of(windows.cbegin(), windows.cend(), [](const QWidget *window) { return window->isVisible(); })) {
                QApplication::quit();
            }
        });
        idle.start();
        return QApplication::exec();
    }
    if (parser.isSet(service)) {
        // D-Bus starts this to answer a call: windows open per call, and the process ends with its last window.
        auto *manager = new FileManager1(context, &app);
        if (!manager->registerOnSessionBus()) {
            return 0;
        }
        // Started but never called: do not linger.
        QTimer idle;
        idle.setSingleShot(true);
        QObject::connect(&idle, &QTimer::timeout, &app, [] {
            if (QApplication::topLevelWindows().isEmpty()) {
                QApplication::quit();
            }
        });
        QObject::connect(manager, &FileManager1::windowOpened, &idle, &QTimer::stop);
        idle.start(30000);
        return QApplication::exec();
    }
    QStringList arguments = parser.positionalArguments();
    if (arguments.isEmpty()) {
        arguments.append(QDir::homePath());
    }
    for (const QString &argument : std::as_const(arguments)) {
        auto *window = new MainWindow(folderFor(argument), context);
        window->setAttribute(Qt::WA_DeleteOnClose);
        window->show();
    }
    return QApplication::exec();
}
