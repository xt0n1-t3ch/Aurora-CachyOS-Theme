# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
"""One fake tray icon (StatusNotifierItem through QSystemTrayIcon) per process, so the sandbox
tray shows the apps of a real desktop with their real icon files. Usage: fake_tray.py NAME ICON_PATH_OR_THEME_NAME"""
import sys

from PyQt6.QtGui import QIcon
from PyQt6.QtWidgets import QApplication, QMenu, QSystemTrayIcon

name, icon = sys.argv[1], sys.argv[2]
app = QApplication(sys.argv)
app.setApplicationName(name)
app.setDesktopFileName(name)
qicon = QIcon(icon) if icon.startswith("/") else QIcon.fromTheme(icon)
tray = QSystemTrayIcon(qicon)
tray.setToolTip(name)
menu = QMenu()
menu.addAction(f"Open {name}")
menu.addAction("Quit")
tray.setContextMenu(menu)
tray.show()
sys.exit(app.exec())
