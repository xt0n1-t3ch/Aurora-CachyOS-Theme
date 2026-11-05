#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Compile all source QML; use an isolated D-Bus session on Linux.
Does not instantiate controls, change hardware, or install the widget.
Requires PySide6 and the Plasma 6 QML modules.
"""
import json
import os
from pathlib import Path
os.environ.setdefault('QT_QPA_PLATFORM', 'offscreen')
from PySide6.QtGui import QGuiApplication
from PySide6.QtQuick import QQuickItem
from PySide6.QtQml import QQmlEngine, QQmlComponent
from PySide6.QtCore import QUrl
app = QGuiApplication([])
engine = QQmlEngine()
source = Path(__file__).resolve().parents[1] / 'package/contents/ui'
errors = {}
files = sorted(source.rglob('*.qml'))
for path in files:
    component = QQmlComponent(engine, QUrl.fromLocalFile(str(path)))
    if component.isError():
        errors[str(path.relative_to(source))] = [e.toString() for e in component.errors()]
print(json.dumps({'files': len(files), 'errors': errors}, indent=2))
raise SystemExit(bool(errors))
