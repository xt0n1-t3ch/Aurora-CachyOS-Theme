// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Aurora Motion asks KWin for the animations of a macOS-like desktop: windows open and close with a short fade and a
// small scale that slows down at the end, never the accelerating shrink of KWin's Scale effect (InCubic to 80 %)
// that looks like the window is swallowed, and minimize into their Dock icon on a smooth in-out curve.
// The script runs against a stand-in of KWin's scripted-effect API, modelled on what KWin 6.8.2's own scale and
// squash effects use (src/plugins/scale and squash, contents/code/main.js).
//
//     bun test components/motion/tests
import { expect, test } from "bun:test";
import { readFileSync } from "node:fs";
import { runInNewContext } from "node:vm";

const SCRIPT = readFileSync(new URL("../package/contents/code/main.js", import.meta.url), "utf8");
const CURVE = { Linear: 0, InCubic: 9, OutCubic: 10, InOutCubic: 11, OutQuint: 18, InOutQuint: 19 };

// A KWin with one window; `requests` collects every animate() call the effect makes.
function kwin(windowProps = {}) {
    const listeners = {};
    const signal = (name) => ({ connect: (fn) => { (listeners[name] ??= []).push(fn); } });
    const requests = [];
    const window = {
        windowClass: "konsole org.kde.konsole", hasDecoration: true, normalWindow: true, managed: true, visible: true,
        minimized: false, geometry: { x: 100, y: 100, width: 800, height: 600 },
        iconGeometry: { x: 1200, y: 1380, width: 48, height: 48 },
        minimizedChanged: signal("minimizedChanged"), setData() {}, ...windowProps,
    };
    const context = {
        effects: { windowAdded: signal("windowAdded"), windowClosed: signal("windowClosed"),
                   windowDataChanged: signal("windowDataChanged"), hasActiveFullScreenEffect: false, stackingOrder: [] },
        effect: { configChanged: signal("configChanged"), animationEnded: signal("animationEnded"),
                  readConfig: (key, fallback) => fallback, isGrabbed: () => false },
        Effect: { Scale: "scale", Opacity: "opacity", Size: "size", Translation: "translation",
                  WindowForceBlurRole: 1, WindowForceBackgroundContrastRole: 2, WindowAddedGrabRole: 3,
                  WindowClosedGrabRole: 4, Forward: 0, Backward: 1 },
        QEasingCurve: CURVE,
        animationTime: (ms) => ms,
        animate: (request) => { requests.push(request); return requests.length; },
        cancel() {}, redirect: () => false,
    };
    runInNewContext(SCRIPT, context);
    const emit = (name, ...args) => (listeners[name] ?? []).forEach((fn) => fn(...args));
    return { window, requests, emit };
}

const part = (request, type) => request.animations.find((animation) => animation.type === type);

test("a window opens with a fade and a small scale that slows down at the end", () => {
    const { window, requests, emit } = kwin();
    emit("windowAdded", window);
    expect(requests).toHaveLength(1);
    expect(requests[0].curve).toBe(CURVE.OutQuint);
    expect(part(requests[0], "scale").from).toBe(0.94);
    expect(part(requests[0], "opacity").from).toBe(0);
});

test("a window closes without being swallowed: it fades and shrinks a little, slowing down", () => {
    const { window, requests, emit } = kwin();
    emit("windowClosed", window);
    expect(requests).toHaveLength(1);
    expect(requests[0].curve).toBe(CURVE.OutCubic);
    expect(requests[0].curve).not.toBe(CURVE.InCubic);
    expect(part(requests[0], "scale").to).toBe(0.96);
    expect(part(requests[0], "opacity").to).toBe(0);
    // Closing is quicker than opening, as on macOS.
    const opening = kwin();
    opening.emit("windowAdded", opening.window);
    expect(requests[0].duration).toBeLessThan(opening.requests[0].duration);
});

test("minimize glides into the Dock icon and restore glides back, on an in-out curve", () => {
    const { window, requests, emit } = kwin();
    emit("windowAdded", window);
    requests.length = 0;
    window.minimized = true;
    emit("minimizedChanged");
    expect(requests).toHaveLength(1);
    expect(requests[0].curve).toBe(CURVE.InOutQuint);
    expect(part(requests[0], "size").to).toEqual({ value1: 48, value2: 48 });
    // The window's centre lands on the icon's centre.
    expect(part(requests[0], "translation").to).toEqual({ value1: 1200 - 100 - (800 - 48) / 2, value2: 1380 - 100 - (600 - 48) / 2 });
    window.minimized = false;
    emit("minimizedChanged");
    expect(requests[1].curve).toBe(CURVE.OutQuint);
    expect(part(requests[1], "opacity").to).toBe(1);
});

test("a window without a Dock icon minimizes without a flight", () => {
    const { window, requests, emit } = kwin({ iconGeometry: { x: 0, y: 0, width: 0, height: 0 } });
    emit("windowAdded", window);
    requests.length = 0;
    window.minimized = true;
    emit("minimizedChanged");
    expect(requests).toHaveLength(0);
});

test("menus, tooltips and the lock screen are left to their own effects", () => {
    for (const props of [{ hasDecoration: false, popupWindow: true }, { hasDecoration: false, lockScreen: true },
                         { windowClass: "ksmserver ksmserver" }]) {
        const { window, requests, emit } = kwin(props);
        emit("windowAdded", window);
        emit("windowClosed", window);
        expect(requests).toHaveLength(0);
    }
});

test("Quick Menu draws its own entrance and fades out quickly when it closes", () => {
    for (const windowClass of ["krunner krunner", "krunner org.kde.krunner"]) {
        const { window, requests, emit } = kwin({ windowClass, hasDecoration: false });
        emit("windowAdded", window);
        expect(requests).toHaveLength(0);
        emit("windowClosed", window);
        expect(requests).toHaveLength(1);
        expect(requests[0].curve).toBe(CURVE.OutCubic);
        expect(part(requests[0], "opacity").to).toBe(0);
        expect(part(requests[0], "scale").to).toBe(0.98);
        expect(requests[0].duration).toBeLessThanOrEqual(160);
    }
});
