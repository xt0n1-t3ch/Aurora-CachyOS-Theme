// SPDX-License-Identifier: GPL-3.0-only
// The Control Center's rules, kept free of Plasma so tests/tst_cc.qml can check them: the macOS Tahoe grid,
// what each module says, and where the window opens.
.pragma library

// Four columns of 62 pt cells with 12 pt gaps, as measured on macOS Tahoe and macOS 27 at 2x.
var CELL = 62;
var GAP = 12;
var COLUMNS = 4;

// The length of a run of cells with the gaps between them.
function span(cells) {
    return cells * CELL + (cells - 1) * GAP;
}

// Where the cell with this index starts, counted from the first cell.
function offset(cell) {
    return cell * (CELL + GAP);
}

// The rectangle a module covers, from its first cell and its size in cells.
function rect(column, row, columns, rows) {
    return {x: offset(column), y: offset(row), width: span(columns), height: span(rows)};
}

// The width of the whole grid.
function gridWidth() {
    return span(COLUMNS);
}

// The default Control Center: [id, column, row, columns, rows]. Tahoe's places, with Linux in Apple's slots:
// Night Light where AirDrop is, Overview for Stage Manager, display modes for Screen Mirroring.
var LAYOUT = [
    ["network", 0, 0, 2, 1],
    ["bluetooth", 0, 1, 2, 1],
    ["nightlight", 0, 2, 2, 1],
    ["media", 2, 0, 2, 2],
    ["overview", 2, 2, 1, 1],
    ["mirroring", 3, 2, 1, 1],
    ["settings", 0, 3, 1, 1],
    ["screenshot", 1, 3, 1, 1],
    ["focus", 2, 3, 2, 1],
    ["display", 0, 4, 4, 1],
    ["sound", 0, 5, 4, 1],
];

// Placements of the modules that are shown, each {id, x, y, width, height}. hidden: ids to leave out; a row left
// empty by them closes up.
function layout(hidden) {
    var shown = LAYOUT.filter(function (entry) {
        return hidden.indexOf(entry[0]) < 0;
    });
    var rows = [];
    shown.forEach(function (entry) {
        for (var row = entry[2]; row < entry[2] + entry[4]; ++row) {
            if (rows.indexOf(row) < 0) {
                rows.push(row);
            }
        }
    });
    rows.sort(function (a, b) {
        return a - b;
    });
    return shown.map(function (entry) {
        var placement = rect(entry[1], rows.indexOf(entry[2]), entry[3], entry[4]);
        placement.id = entry[0];
        return placement;
    });
}

// The height of the grid: the lowest bottom edge of the placements.
function heightOf(placements) {
    return placements.reduce(function (height, p) {
        return Math.max(height, p.y + p.height);
    }, 0);
}

// The placement of the module with this id, or null when it is not shown.
function placementOf(placements, id) {
    for (var i = 0; i < placements.length; ++i) {
        if (placements[i].id === id) {
            return placements[i];
        }
    }
    return null;
}

// "HomeNet: Connected" (Plasma's NetworkStatus text, one line per active connection) gives "HomeNet".
function connectionName(status) {
    var first = (status || "").split("\n")[0];
    var colon = first.lastIndexOf(":");
    return colon > 0 ? first.substring(0, colon).trim() : first.trim();
}

// The network pill. state: {wifi: has a Wi-Fi device, wifiOn, icon: Plasma's connection icon name, status}.
function networkSummary(state) {
    var icon = state.icon || "";
    var wired = icon.indexOf("wired") >= 0;
    var wireless = icon.indexOf("wireless") >= 0 && icon.indexOf("disconnected") < 0;
    var linked = (wired || wireless) && icon.indexOf("disconnect") < 0;
    if (state.wifi && !wired) {
        if (!state.wifiOn) {
            return {title: "Wi-Fi", subtitle: "Off", glyph: "wifi", active: false};
        }
        return {title: "Wi-Fi", subtitle: linked ? connectionName(state.status) : "Not Connected", glyph: "wifi", active: true};
    }
    return {title: "Ethernet", subtitle: linked ? "Connected" : "Not Connected", glyph: "ethernet", active: linked};
}

// The Bluetooth pill. state: {adapters: count, operational, connected: names of connected devices}.
function bluetoothSummary(state) {
    if (state.adapters <= 0) {
        return {subtitle: "Unavailable", active: false, available: false};
    }
    if (!state.operational) {
        return {subtitle: "Off", active: false, available: true};
    }
    return {subtitle: state.connected.length ? state.connected[0] : "On", active: true, available: true};
}

// A volume as a whole percent of the normal volume; anything louder than normal shows as 100.
function percentFromVolume(volume, normal) {
    return Math.round(Math.max(0, Math.min(1, volume / normal)) * 100);
}

// The volume for a percent from 0 to 100 of the normal volume.
function volumeFromPercent(percent, normal) {
    return Math.round(Math.max(0, Math.min(100, percent)) / 100 * normal);
}

// The speaker glyph for a volume: the slashed one when muted or at zero.
function speakerGlyph(percent, muted) {
    return muted || percent <= 0 ? "speaker.slash" : "speaker";
}

// One Display slider for all monitors: it shows their average and sets each to the same share of its range.
// displays: [{name, current, maximum}]; monitors without a range are left alone. -1 means no slider.
function brightnessPercent(displays) {
    var usable = displays.filter(function (d) {
        return d.maximum > 0;
    });
    if (!usable.length) {
        return -1;
    }
    var sum = usable.reduce(function (total, d) {
        return total + d.current / d.maximum;
    }, 0);
    return Math.round(sum / usable.length * 100);
}

// The value to set on each monitor that has a range, in its own units, for a slider percent from 0 to 100.
function brightnessTargets(percent, displays) {
    return displays.filter(function (d) {
        return d.maximum > 0;
    }).map(function (d) {
        return {name: d.name, value: Math.round(Math.max(0, Math.min(100, percent)) / 100 * d.maximum)};
    });
}

// A press on the menu bar icon can first take focus from the open window, which then hides itself; the click
// that follows must not open it again. 250 ms covers the press-to-click time with room to spare.
function opensOnClick(visible, msSinceHidden) {
    return !visible && msSinceHidden > 250;
}

// The transparent window margin around the modules. Top and right are the macOS gaps to the menu bar (6 px) and
// to the screen's edge (10 px): KWin keeps popups out of the bar's area, and past the edge is the next screen.
// Left and bottom leave room for the modules' shadows.
var MARGINS = {top: 6, right: 10, bottom: 28, left: 28};

// The window starts right under the menu bar and ends at the screen's right edge. screen: {x, y, width, height}.
function windowPosition(screen, barHeight, windowWidth) {
    return {x: screen.x + screen.width - windowWidth, y: screen.y + barHeight};
}

// Now Playing. player: {track, artist, identity} or null.
function mediaSummary(player) {
    if (!player) {
        return {title: "Not Playing", artist: "", hasPlayer: false};
    }
    return {title: player.track || player.identity || "Not Playing", artist: player.artist || "", hasPlayer: true};
}

// Where a pill's title and subtitle go: after the round switch (centred in the pill's height, as on macOS) and a 6 px
// gap, up to 10 px before the pill's end, where its rounded edge starts to curve in.
var PILL_TEXT_GAP = 6;
var PILL_TEXT_END = 10;
function pillText(width, height, toggleSize) {
    var x = (height - toggleSize) / 2 + toggleSize + PILL_TEXT_GAP;
    return {x: x, width: width - x - PILL_TEXT_END};
}

// How the window opens, dropping from the menu bar like macOS's: a fade over Kirigami's long duration, then a small
// scale that slows down at the end over a little longer. `longDuration` already carries the desktop's animation
// speed, so instant animations (0) open the window at once.
function entrance(longDuration) {
    return {
        scaleFrom: 0.95,
        fadeDuration: longDuration,
        scaleDuration: Math.round(longDuration * 1.4)
    };
}
