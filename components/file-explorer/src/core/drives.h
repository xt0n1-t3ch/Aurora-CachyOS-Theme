// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Drive names, order, capacity and symbols for the sidebar, with no widgets so they can be tested on their own.

#pragma once

#include <QList>
#include <QMap>
#include <QString>

#include <functional>

namespace fe {

/// What a drive is, for its symbol and its place in the sidebar.
enum class DriveKind {
    /// The disk mounted at /.
    System,
    /// Built into the machine.
    Internal,
    /// A disk on a cable: USB, Thunderbolt.
    External,
    /// A USB flash drive.
    Stick,
    Optical,
    /// An SD or other memory card.
    Card,
    Network,
};

/// A drive of the sidebar, as KDE's places and Solid describe it.
struct Drive {
    /// Where the user's own name for it is kept: "system" for the disk mounted at /, else
    /// "uuid-<filesystem UUID>", else "dev-<block device>", else "path-<mount point>".
    QString key;
    /// KDE's name for it: the filesystem label, or a description such as "476.9 GiB Internal Drive".
    QString label;
    DriveKind kind = DriveKind::Internal;
};

/// The computer's name as the system disk's name, as macOS writes "Macintosh HD": the host name with its
/// dashes and underscores as spaces ("work-laptop" is "work laptop"), or "System" without one.
[[nodiscard]] QString systemDriveName(const QString &hostName);

/// The name a drive shows: the name the user gave it, else the computer's name for the system disk, else KDE's.
[[nodiscard]] QString driveName(const Drive &drive, const QMap<QString, QString> &names, const QString &hostName);

/// A name typed in Rename, tidied: trimmed, runs of white space as one space, at most 64 characters. An empty
/// result gives the drive its default name back.
[[nodiscard]] QString cleanDriveName(const QString &typed);

/// The sidebar's order of drives, as positions in drives: the system disk, the drives built into the machine,
/// the removable ones, then network drives; by name within each, numbers as numbers and letters without case.
[[nodiscard]] QList<qsizetype> driveOrder(const QList<Drive> &drives, const QMap<QString, QString> &names, const QString &hostName);

/// How full a drive is.
struct Capacity {
    enum class Level {
        Roomy,
        /// Over 90 % used: the bar turns amber.
        Low,
        /// Over 95 % used, where Dolphin turns its bar red (kfileplacesview.cpp, KIO 6.31).
        Critical,
    };
    /// The used share, from 0 to 1.
    qreal used = 0;
    Level level = Level::Roomy;
};

/// size and available in bytes; an unknown size gives an empty bar.
[[nodiscard]] Capacity capacityFor(quint64 size, quint64 available);

/// "212 GB free of 1.9 TB", in the sizes formatSize writes.
[[nodiscard]] QString capacityText(quint64 size, quint64 available, const std::function<QString(qint64)> &formatSize);

/// The app's own symbol for a kind of drive (resources/icons/places).
[[nodiscard]] QString driveSymbol(DriveKind kind);

} // namespace fe
