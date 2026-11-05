/*
    SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
    SPDX-FileCopyrightText: 2018, 2021 Vlad Zahorodnii <vlad.zahorodnii@kde.org>
    SPDX-License-Identifier: GPL-3.0-only
    Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.

    Aurora Motion: how windows open, close, minimize and come back, timed and curved like macOS. Built on KWin's own
    Scale and Squash effects (GPL-2.0-or-later, used here under GPL-3.0): the same window rules and animate() calls,
    with curves that slow down at the end. KWin's Scale closes on InCubic to 80 %, which looks like the window is
    swallowed; here it fades and shrinks to 96 % while slowing down.
*/

"use strict";

// Windows that other effects animate, or that must not animate (kwin src/plugins/scale).
const blacklist = {
    "ksmserver ksmserver": [],
    "ksmserver-logout-greeter ksmserver-logout-greeter": [],
    "ksplashqml ksplashqml": [],
    "spectacle org.kde.spectacle": ["region-editor"],
};

// Milliseconds before the desktop's animation speed scales them, and the sizes windows start and end at.
const OPEN = 260;
const CLOSE = 200;
const MINIMIZE = 340;
const QUICK_MENU_CLOSE = 160;
const OPEN_SCALE = 0.94;
const CLOSE_SCALE = 0.96;
const QUICK_MENU_SCALE = 0.98;

class AuroraMotion {
    constructor() {
        effect.configChanged.connect(this.loadConfig.bind(this));
        effect.animationEnded.connect(this.cleanupForcedRoles.bind(this));
        effects.windowAdded.connect(this.slotWindowAdded.bind(this));
        effects.windowClosed.connect(this.slotWindowClosed.bind(this));
        effects.windowDataChanged.connect(this.slotWindowDataChanged.bind(this));
        for (const window of effects.stackingOrder) {
            this.watchMinimize(window);
        }
        this.loadConfig();
    }

    loadConfig() {
        this.openDuration = animationTime(effect.readConfig("OpenDuration", OPEN) || OPEN);
        this.closeDuration = animationTime(effect.readConfig("CloseDuration", CLOSE) || CLOSE);
        this.minimizeDuration = animationTime(effect.readConfig("MinimizeDuration", MINIMIZE) || MINIMIZE);
        this.quickMenuDuration = animationTime(QUICK_MENU_CLOSE);
    }

    // Quick Menu (Aurora's KRunner) draws its own entrance in QML; its window only fades out here.
    static isQuickMenu(window) {
        const [name, appClass] = String(window.windowClass).split(" ");
        return name == "krunner" && (appClass == "krunner" || appClass == "org.kde.krunner");
    }

    // Normal windows and dialogs; plasmashell's own surfaces only when decorated (its settings windows).
    static animates(window) {
        if (window.windowClass == "plasmashell plasmashell" || window.windowClass == "plasmashell org.kde.plasmashell") {
            return window.hasDecoration;
        }
        const tags = blacklist[window.windowClass];
        if (tags && (tags.length === 0 || tags.includes(window.tag))) {
            return false;
        }
        if (window.hasDecoration) {
            return true;
        }
        if (window.popupWindow || window.lockScreen || window.outline || !window.managed) {
            return false;
        }
        return window.normalWindow || window.dialog;
    }

    setupForcedRoles(window) {
        window.setData(Effect.WindowForceBackgroundContrastRole, true);
        window.setData(Effect.WindowForceBlurRole, true);
    }

    cleanupForcedRoles(window) {
        window.setData(Effect.WindowForceBackgroundContrastRole, null);
        window.setData(Effect.WindowForceBlurRole, null);
    }

    slotWindowAdded(window) {
        this.watchMinimize(window);
        if (AuroraMotion.isQuickMenu(window)) {
            return;
        }
        if (effects.hasActiveFullScreenEffect || !AuroraMotion.animates(window) || !window.visible
                || effect.isGrabbed(window, Effect.WindowAddedGrabRole)) {
            return;
        }
        this.setupForcedRoles(window);
        window.auroraOpen = animate({
            window: window,
            curve: QEasingCurve.OutQuint,
            duration: this.openDuration,
            animations: [{ type: Effect.Scale, from: OPEN_SCALE }, { type: Effect.Opacity, from: 0 }],
        });
    }

    slotWindowClosed(window) {
        if (AuroraMotion.isQuickMenu(window) && window.visible && !effects.hasActiveFullScreenEffect) {
            window.auroraClose = animate({
                window: window,
                curve: QEasingCurve.OutCubic,
                duration: this.quickMenuDuration,
                animations: [{ type: Effect.Scale, to: QUICK_MENU_SCALE }, { type: Effect.Opacity, to: 0 }],
            });
            return;
        }
        if (effects.hasActiveFullScreenEffect || !AuroraMotion.animates(window) || !window.visible
                || window.skipsCloseAnimation || effect.isGrabbed(window, Effect.WindowClosedGrabRole)) {
            return;
        }
        if (window.auroraOpen) {
            cancel(window.auroraOpen);
            delete window.auroraOpen;
        }
        this.setupForcedRoles(window);
        window.auroraClose = animate({
            window: window,
            curve: QEasingCurve.OutCubic,
            duration: this.closeDuration,
            animations: [{ type: Effect.Scale, to: CLOSE_SCALE }, { type: Effect.Opacity, to: 0 }],
        });
    }

    slotWindowDataChanged(window, role) {
        const running = role == Effect.WindowAddedGrabRole ? "auroraOpen"
            : role == Effect.WindowClosedGrabRole ? "auroraClose" : null;
        if (running && window[running] && effect.isGrabbed(window, role)) {
            cancel(window[running]);
            delete window[running];
            this.cleanupForcedRoles(window);
        }
    }

    watchMinimize(window) {
        if (window.auroraWatched) {
            return;
        }
        window.auroraWatched = true;
        window.minimizedChanged.connect(() => this.slotMinimizedChanged(window));
    }

    // The window's size and the offset that puts its centre on the Dock icon's centre (kwin src/plugins/squash).
    static flight(window) {
        const icon = window.iconGeometry;
        const frame = window.geometry;
        return {
            size: { value1: frame.width, value2: frame.height },
            iconSize: { value1: icon.width, value2: icon.height },
            home: { value1: 0, value2: 0 },
            dock: {
                value1: icon.x - frame.x - (frame.width - icon.width) / 2,
                value2: icon.y - frame.y - (frame.height - icon.height) / 2,
            },
        };
    }

    slotMinimizedChanged(window) {
        if (effects.hasActiveFullScreenEffect) {
            return;
        }
        const icon = window.iconGeometry;
        if (!icon || icon.width == 0 || icon.height == 0) {
            return;
        }
        const leaving = window.minimized;
        const [mine, other] = leaving ? ["auroraMinimize", "auroraRestore"] : ["auroraRestore", "auroraMinimize"];
        // Turning around halfway runs the other animation backwards, from where the window is now.
        if (window[other]) {
            if (redirect(window[other], Effect.Backward)) {
                return;
            }
            cancel(window[other]);
            delete window[other];
        }
        if (window[mine]) {
            if (redirect(window[mine], Effect.Forward)) {
                return;
            }
            cancel(window[mine]);
        }
        const path = AuroraMotion.flight(window);
        window.setData(Effect.WindowForceBlurRole, true);
        window[mine] = animate({
            window: window,
            curve: leaving ? QEasingCurve.InOutQuint : QEasingCurve.OutQuint,
            duration: this.minimizeDuration,
            keepAlive: false,
            animations: [
                { type: Effect.Size, from: leaving ? path.size : path.iconSize, to: leaving ? path.iconSize : path.size },
                { type: Effect.Translation, from: leaving ? path.home : path.dock, to: leaving ? path.dock : path.home },
                { type: Effect.Opacity, from: leaving ? 1 : 0, to: leaving ? 0 : 1 },
            ],
        });
    }
}

new AuroraMotion();
