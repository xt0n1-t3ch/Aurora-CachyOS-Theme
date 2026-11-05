// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// How the menu bar clock writes the date and time, like macOS: "Wed Nov 4" and "8:01 AM", or "mié 4 nov" and
// "8:01 a. m." in Spanish. names: {days: Sunday first, months, am, pm, dayFirst, uses24h, padHours}.
.pragma library

// macOS writes "mié", not CLDR's "mié.".
function shortName(name) {
    return name.replace(/\.$/, "");
}

// The date as macOS writes it in the menu bar: "Wed Nov 4", or "mié 4 nov" when the day comes first.
function dateText(date, names) {
    var day = shortName(names.days[date.getDay()]);
    var month = shortName(names.months[date.getMonth()]);
    return names.dayFirst ? day + " " + date.getDate() + " " + month : day + " " + month + " " + date.getDate();
}

// The time: "8:01 AM" on a 12 hour clock, "20:05" on a 24 hour one, with "07:05" when the region pads hours.
function timeText(date, names) {
    var minutes = (date.getMinutes() < 10 ? "0" : "") + date.getMinutes();
    var hours = date.getHours();
    if (names.uses24h) {
        return (names.padHours && hours < 10 ? "0" : "") + hours + ":" + minutes;
    }
    return (hours % 12 || 12) + ":" + minutes + " " + (hours < 12 ? names.am : names.pm);
}

// setting: Plasma's digital clock values, 0 = 12 h, 1 = the region's (its short time format), 2 = 24 h. A region
// format with an am/pm marker ("ap" in Qt's format letters) is a 12 hour one.
function uses24h(setting, regionTimeFormat) {
    if (setting === 0) {
        return false;
    }
    if (setting === 2) {
        return true;
    }
    return regionTimeFormat.toLowerCase().indexOf("ap") < 0;
}
