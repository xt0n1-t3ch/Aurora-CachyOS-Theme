#!/bin/bash
# SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
# SPDX-License-Identifier: GPL-3.0-only
# Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
# run.sh SCENARIO [TIMEOUT]: start an isolated headless Plasma (KWin's virtual backend, its own HOME and D-Bus)
# and run SCENARIO inside it. Nothing touches the real session or its settings. Pass the scenario's full path.
# TIMEOUT is in seconds (default 120). SANDBOX_SCALE=2 draws the 2560x1440 screen at twice the pixels, for sharp
# captures. Logs go to logs/ and captures to shots/ beside this script.
SBX=$(dirname "$(readlink -f "$0")")
SB=$(readlink -f "$SBX/../..")
SCEN="$1"; TO="${2:-120}"
# The scenarios reach a few files of the real home (app icons, a wallpaper) through REALHOME.
REALHOME="$HOME"
mkdir -p "$SBX/run" "$SBX/logs" "$SBX/shots" "$SBX/home"
chmod 700 "$SBX/run"
cd "$SB" || exit 1
# env -i drops the live session's environment: only the variables listed here reach the sandbox.
exec timeout "$TO" env -i HOME="$SBX/home" REALHOME="$REALHOME" USER="$USER" LOGNAME="$USER" \
  PATH=/usr/local/bin:/usr/bin:/usr/sbin LANG="${LANG:-en_US.UTF-8}" \
  XDG_RUNTIME_DIR="$SBX/run" XDG_CONFIG_HOME="$SBX/home/.config" XDG_DATA_HOME="$SBX/home/.local/share" \
  XDG_CACHE_HOME="$SBX/home/.cache" XDG_STATE_HOME="$SBX/home/.local/state" \
  XDG_DATA_DIRS=/usr/local/share:/usr/share XDG_CONFIG_DIRS=/etc/xdg \
  XDG_SESSION_TYPE=wayland XDG_CURRENT_DESKTOP=KDE KDE_FULL_SESSION=true KDE_SESSION_VERSION=6 \
  QT_PLUGIN_PATH="$SBX/home/.local/lib/qt6/plugins:/usr/lib/qt6/plugins" \
  QML_IMPORT_PATH="$SBX/home/.local/lib/qml:$SBX/home/.local/lib/qt6/qml" SB="$SB" SBX="$SBX" \
  KV_NAME="$KV_NAME" SCEN_TAG="$SCEN_TAG" POINTER_DEBUG="$POINTER_DEBUG" SANDBOX_SCALE="${SANDBOX_SCALE:-1}" \
  dbus-run-session -- kwin_wayland --virtual --width 2560 --height 1440 --scale "${SANDBOX_SCALE:-1}" --socket wayland-sbx --xwayland \
  --no-lockscreen --exit-with-session "$SCEN"
