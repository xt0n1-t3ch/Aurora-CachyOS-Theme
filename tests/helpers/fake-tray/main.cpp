// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// fake-tray NAME ICON [BADGE]: one tray icon with a real app's icon, so the sandbox menu bar shows
// the background apps of a real desktop. ICON is a file path or a theme icon name; BADGE draws an unread count.
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QPainter>
#include <QSystemTrayIcon>

#include <cstdio>

namespace {

// Returns icon with a red dot in its top right corner. A BADGE other than the bullet is drawn as text inside the dot;
// an empty badge returns icon unchanged.
QIcon withBadge(const QIcon &icon, const QString &badge)
{
    if (badge.isEmpty()) {
        return icon;
    }
    QPixmap pixmap = icon.pixmap(64, 64);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(237, 66, 69));
    const QRectF dot(pixmap.width() * 0.52, 0, pixmap.width() * 0.48, pixmap.height() * 0.48);
    painter.drawEllipse(dot);
    if (badge != u"•") {
        QFont font;
        font.setPixelSize(pixmap.height() / 4);
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(dot, Qt::AlignCenter, badge);
    }
    painter.end();
    return QIcon(pixmap);
}

} // namespace

// Shows the tray icon until the process is killed. Returns 2 when NAME or ICON is missing.
int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    if (argc < 3) {
        qCritical("usage: fake-tray NAME ICON [BADGE]");
        return 2;
    }
    const QString name = QString::fromLocal8Bit(argv[1]);
    const QString source = QString::fromLocal8Bit(argv[2]);
    QApplication::setApplicationName(name);
    QApplication::setDesktopFileName(name);
    const QIcon icon = source.startsWith(u'/') ? QIcon(source) : QIcon::fromTheme(source);
    QSystemTrayIcon tray(withBadge(icon, argc > 3 ? QString::fromLocal8Bit(argv[3]) : QString()));
    tray.setToolTip(name);
    QMenu menu;
    menu.addAction(QStringLiteral("Open %1").arg(name));
    menu.addSeparator();
    menu.addAction(QStringLiteral("Quit %1").arg(name));
    tray.setContextMenu(&menu);
    // Scenarios read this line to know that a click on the icon reached the app.
    QObject::connect(&tray, &QSystemTrayIcon::activated, [](QSystemTrayIcon::ActivationReason reason) {
        std::printf("activated %d\n", static_cast<int>(reason));
        std::fflush(stdout);
    });
    tray.show();
    return QApplication::exec();
}
