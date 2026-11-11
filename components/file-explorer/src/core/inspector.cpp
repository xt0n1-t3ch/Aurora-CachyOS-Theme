// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements inspector.h.

#include "core/inspector.h"

#include <QElapsedTimer>
#include <QFile>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>

#include <fts.h>
#include <sys/stat.h>

#include <cstring>
#include <vector>

namespace fe {

namespace {

/// A block size that keeps the disk busy without holding much memory.
constexpr qint64 HASH_BLOCK = 1 << 20;
/// st_blocks counts 512-byte units whatever the filesystem's block size (stat(2)).
constexpr qint64 STAT_BLOCK = 512;

} // namespace

bool tallyTree(const QStringList &paths, const std::atomic_bool &cancel, Tally &tally,
               const std::function<void(const Tally &)> &progress, int progressEveryMs)
{
    if (paths.isEmpty()) {
        return true;
    }
    std::vector<QByteArray> encoded;
    encoded.reserve(size_t(paths.size()));
    std::vector<char *> argv;
    for (const QString &path : paths) {
        encoded.push_back(QFile::encodeName(path));
    }
    for (QByteArray &path : encoded) {
        argv.push_back(path.data());
    }
    argv.push_back(nullptr);
    // FTS_PHYSICAL does not follow links, FTS_XDEV stays on each root's filesystem, FTS_NOCHDIR leaves the working
    // folder alone, which the rest of the app shares.
    FTS *tree = ::fts_open(argv.data(), FTS_PHYSICAL | FTS_XDEV | FTS_NOCHDIR, nullptr);
    if (!tree) {
        return true;
    }
    QSet<QPair<quint64, quint64>> linked;
    QElapsedTimer sinceReport;
    sinceReport.start();
    bool finished = true;
    while (FTSENT *node = ::fts_read(tree)) {
        if (cancel.load(std::memory_order_relaxed)) {
            finished = false;
            break;
        }
        switch (node->fts_info) {
        case FTS_D:
            if (node->fts_level > 0) {
                ++tally.folders;
            }
            break;
        case FTS_DNR:
        case FTS_ERR:
            ++tally.unreadable;
            break;
        case FTS_DP:
        case FTS_DC:
            // The second visit of a folder, and a folder that loops back to one above it, add nothing.
            break;
        case FTS_NS:
        case FTS_NSOK:
            ++tally.files;
            break;
        default: {
            const struct stat *info = node->fts_statp;
            if (info->st_nlink > 1 && !S_ISDIR(info->st_mode)) {
                const QPair<quint64, quint64> key(quint64(info->st_dev), quint64(info->st_ino));
                if (linked.contains(key)) {
                    break;
                }
                linked.insert(key);
            }
            ++tally.files;
            tally.bytes += qint64(info->st_size);
            tally.onDisk += qint64(info->st_blocks) * STAT_BLOCK;
            break;
        }
        }
        if (progress && sinceReport.elapsed() >= progressEveryMs) {
            progress(tally);
            sinceReport.restart();
        }
    }
    ::fts_close(tree);
    return finished;
}

QString tallyText(const Tally &tally, bool done, const std::function<QString(qint64)> &formatSize)
{
    const QLocale locale;
    const qint64 items = tally.files + tally.folders;
    QString text = QStringLiteral("%1 (%2 on disk), %3")
                       .arg(formatSize(tally.bytes), formatSize(tally.onDisk),
                            items == 1 ? QStringLiteral("1 item") : QStringLiteral("%1 items").arg(locale.toString(items)));
    if (tally.unreadable > 0) {
        text += tally.unreadable == 1 ? QStringLiteral("; 1 folder could not be read")
                                      : QStringLiteral("; %1 folders could not be read").arg(locale.toString(tally.unreadable));
    }
    return done ? text : QStringLiteral("Counting: ") + text;
}

QList<HashKind> hashKinds()
{
    return {{QCryptographicHash::Sha256, QStringLiteral("SHA-256")},
            {QCryptographicHash::Sha512, QStringLiteral("SHA-512")},
            {QCryptographicHash::Sha1, QStringLiteral("SHA-1")},
            {QCryptographicHash::Md5, QStringLiteral("MD5")}};
}

std::optional<QString> hashFile(const QString &path, QCryptographicHash::Algorithm algorithm, const std::atomic_bool &cancel,
                                const std::function<void(qint64)> &progress)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return std::nullopt;
    }
    QCryptographicHash hash(algorithm);
    QByteArray block(HASH_BLOCK, Qt::Uninitialized);
    qint64 done = 0;
    for (;;) {
        if (cancel.load(std::memory_order_relaxed)) {
            return std::nullopt;
        }
        const qint64 got = file.read(block.data(), block.size());
        if (got < 0) {
            return std::nullopt;
        }
        if (got == 0) {
            break;
        }
        hash.addData(QByteArrayView(block.constData(), got));
        done += got;
        if (progress) {
            progress(done);
        }
    }
    return QString::fromLatin1(hash.result().toHex());
}

HashCheck checkHash(const QString &computedHex, const QString &typed)
{
    QString text = typed.trimmed();
    if (text.isEmpty()) {
        return HashCheck::Empty;
    }
    // A line of sha256sum and its kin: the hash, a space, then " name" or "*name".
    static const QRegularExpression line(QStringLiteral("^([0-9A-Fa-f]+)\\s+\\*?.*$"));
    if (const QRegularExpressionMatch match = line.match(text); match.hasMatch()) {
        text = match.captured(1);
    }
    text.remove(QLatin1Char(' '));
    const QString wanted = text.toLower();
    static const QRegularExpression hex(QStringLiteral("^[0-9a-f]+$"));
    if (wanted.size() != computedHex.size() || !hex.match(wanted).hasMatch()) {
        return HashCheck::NotThisKind;
    }
    return wanted == computedHex.toLower() ? HashCheck::Match : HashCheck::Mismatch;
}

QString permissionsText(uint mode)
{
    QString text(9, QLatin1Char('-'));
    const uint bits[9] = {S_IRUSR, S_IWUSR, S_IXUSR, S_IRGRP, S_IWGRP, S_IXGRP, S_IROTH, S_IWOTH, S_IXOTH};
    const char marks[] = "rwxrwxrwx";
    for (int i = 0; i < 9; ++i) {
        if (mode & bits[i]) {
            text[i] = QLatin1Char(marks[i]);
        }
    }
    // The set-id and sticky bits replace the x they go with: s or t with x, S or T without (ls(1)).
    const auto special = [&text, mode](int at, uint bit, char withX, char withoutX) {
        if (mode & bit) {
            text[at] = QLatin1Char(text.at(at) == QLatin1Char('x') ? withX : withoutX);
        }
    };
    special(2, S_ISUID, 's', 'S');
    special(5, S_ISGID, 's', 'S');
    special(8, S_ISVTX, 't', 'T');
    return QStringLiteral("%1 (%2)").arg(text, QString::number(mode & 07777, 8));
}

} // namespace fe
