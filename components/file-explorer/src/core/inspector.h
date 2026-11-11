// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// What the Inspector works out about items without any widget: the size of folders, file hashes, and the texts it
// shows for them. The walking and hashing run on a worker thread; everything here can be cancelled.

#pragma once

#include <QCryptographicHash>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <optional>

namespace fe {

/// What a walk of some items found.
struct Tally {
    /// Files, links and other non-folders, each counted once even when hard-linked several times.
    qint64 files = 0;
    /// Folders inside the items; a folder that is one of the items is not counted.
    qint64 folders = 0;
    /// The sum of the files' sizes, as their length in bytes.
    qint64 bytes = 0;
    /// The space their blocks take on disk; -1 when it is not known (items on another machine or in an archive).
    qint64 onDisk = 0;
    /// Folders that could not be read; their contents are missing from the sums.
    qint64 unreadable = 0;

    friend bool operator==(const Tally &, const Tally &) = default;
};

/// Walks paths and everything inside them into tally, the way `du` does: it stays on each path's filesystem (another
/// disk mounted inside is not counted), does not follow links (a link counts as a file of the link's own size), and
/// counts a file with several hard links once. progress, when set, receives the sums so far at most every
/// progressEveryMs milliseconds. Returns false when cancel became true before the walk ended.
bool tallyTree(const QStringList &paths, const std::atomic_bool &cancel, Tally &tally,
               const std::function<void(const Tally &)> &progress = {}, int progressEveryMs = 100);

/// The Inspector's size line: "3.4 GB (3.5 GB on disk), 1,204 items" or, while the walk runs, "Counting: ...".
/// Unreadable folders are named. Sizes are written by formatSize; counts with the locale's digit grouping.
[[nodiscard]] QString tallyText(const Tally &tally, bool done, const std::function<QString(qint64)> &formatSize);

/// The hashes the Inspector offers, with their names as download pages write them.
struct HashKind {
    QCryptographicHash::Algorithm algorithm;
    QString name;
};
[[nodiscard]] QList<HashKind> hashKinds();

/// The hash of the file at path in lowercase hex, read in blocks; progress receives the bytes read so far. Nothing
/// when the file cannot be read or cancel became true.
[[nodiscard]] std::optional<QString> hashFile(const QString &path, QCryptographicHash::Algorithm algorithm,
                                              const std::atomic_bool &cancel,
                                              const std::function<void(qint64)> &progress = {});

enum class HashCheck {
    /// Nothing typed.
    Empty,
    Match,
    Mismatch,
    /// The text holds no hash of this length.
    NotThisKind,
};

/// Compares a typed or pasted hash with a computed one. Case, spaces, and the file name and asterisk of a
/// `sha256sum` line ("<hash>  name", "<hash> *name") are ignored.
[[nodiscard]] HashCheck checkHash(const QString &computedHex, const QString &typed);

/// Permissions as `ls -l` writes them, then in octal: "rwxr-xr-x (755)". mode is st_mode; the set-id and sticky
/// bits show as `ls` shows them.
[[nodiscard]] QString permissionsText(uint mode);

} // namespace fe
