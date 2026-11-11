// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The KIO worker of `aurora-search:` URLs: lists the matches of a query from the aurora-search helper, so Dolphin's
// view shows them like any folder (icons, columns, drag and drop, context menus). URL: aurora-search:?q=TEXT with an
// optional scope=FOLDER and title=TITLE (fe::indexedSearchUrl).

#include <KIO/WorkerBase>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QUrlQuery>

#include <cerrno>
#include <cstring>
#include <ctime>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

/// The most matches one listing shows; past it the view would only grow slower to read.
constexpr int LISTING_LIMIT = 1000;
/// How long a listing waits for the helper, in milliseconds: a first start may index for a while, but answers from
/// what it has at once.
constexpr int ANSWER_MS = 5000;
constexpr int MESSAGE_MAX = 64 * 1024;

QString socketPath()
{
    return qEnvironmentVariable("AURORA_SEARCH_SOCKET", QStringLiteral("/run/aurora/search.sock"));
}

/// Seconds since the epoch at the start of today, in the local time zone, for `dm:today`.
qint64 startOfToday()
{
    const time_t now = time(nullptr);
    tm local{};
    localtime_r(&now, &local);
    local.tm_hour = 0;
    local.tm_min = 0;
    local.tm_sec = 0;
    return qint64(mktime(&local));
}

class SearchWorker : public KIO::WorkerBase
{
public:
    SearchWorker(const QByteArray &pool, const QByteArray &app)
        : KIO::WorkerBase(QByteArrayLiteral("aurora-search"), pool, app)
    {
    }

    KIO::WorkerResult stat(const QUrl &url) override
    {
        KIO::UDSEntry entry;
        entry.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
        entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, QUrlQuery(url).queryItemValue(QStringLiteral("title"), QUrl::FullyDecoded));
        entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, 0500);
        statEntry(entry);
        return KIO::WorkerResult::pass();
    }

    KIO::WorkerResult listDir(const QUrl &url) override
    {
        const QUrlQuery query(url);
        const QString text = query.queryItemValue(QStringLiteral("q"), QUrl::FullyDecoded);
        const QString scope = query.queryItemValue(QStringLiteral("scope"), QUrl::FullyDecoded);

        const int fd = connectToHelper();
        if (fd < 0) {
            return KIO::WorkerResult::fail(KIO::ERR_CANNOT_CONNECT, QStringLiteral("File Explorer's search"));
        }
        const qint64 now = qint64(time(nullptr));
        QByteArray message = "query\t1\t" + QByteArray::number(LISTING_LIMIT) + '\t' + QByteArray::number(now) + '\t'
            + QByteArray::number(startOfToday()) + '\t' + QFile::encodeName(scope) + '\t' + text.toUtf8();
        if (::send(fd, message.constData(), size_t(message.size()), MSG_NOSIGNAL) < 0) {
            ::close(fd);
            return KIO::WorkerResult::fail(KIO::ERR_CANNOT_CONNECT, QStringLiteral("File Explorer's search"));
        }
        const QByteArray reply = receiveAnswer(fd);
        ::close(fd);
        const qsizetype newline = reply.indexOf('\n');
        const QList<QByteArray> header = reply.left(newline).split('\t');
        if (newline < 0 || header.size() < 6 || header.at(0) != "result") {
            return KIO::WorkerResult::fail(KIO::ERR_CANNOT_CONNECT, QStringLiteral("File Explorer's search"));
        }
        if (!header.at(5).isEmpty()) {
            return KIO::WorkerResult::fail(KIO::ERR_WORKER_DEFINED, QString::fromUtf8(header.at(5)));
        }

        KIO::UDSEntry self;
        self.fastInsert(KIO::UDSEntry::UDS_NAME, QStringLiteral("."));
        self.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, S_IFDIR);
        listEntry(self);
        int number = 0;
        for (const QByteArray &record : reply.mid(newline + 1).split('\0')) {
            if (record.size() < 3 || record.at(1) != '\t') {
                continue;
            }
            const QByteArray path = record.mid(2);
            struct stat info {};
            // Gone since the helper looked: not listed, so no row points at nothing.
            if (::lstat(path.constData(), &info) != 0) {
                continue;
            }
            const QString local = QFile::decodeName(path);
            KIO::UDSEntry entry;
            // A name unique within the listing; the view shows the display name and acts on the URL.
            entry.fastInsert(KIO::UDSEntry::UDS_NAME, QString::number(++number));
            entry.fastInsert(KIO::UDSEntry::UDS_DISPLAY_NAME, local.section(QLatin1Char('/'), -1));
            entry.fastInsert(KIO::UDSEntry::UDS_URL, QUrl::fromLocalFile(local).toString());
            entry.fastInsert(KIO::UDSEntry::UDS_LOCAL_PATH, local);
            entry.fastInsert(KIO::UDSEntry::UDS_FILE_TYPE, info.st_mode & S_IFMT);
            entry.fastInsert(KIO::UDSEntry::UDS_ACCESS, info.st_mode & 07777);
            entry.fastInsert(KIO::UDSEntry::UDS_SIZE, info.st_size);
            entry.fastInsert(KIO::UDSEntry::UDS_MODIFICATION_TIME, info.st_mtim.tv_sec);
            entry.fastInsert(KIO::UDSEntry::UDS_ACCESS_TIME, info.st_atim.tv_sec);
            entry.fastInsert(KIO::UDSEntry::UDS_LOCAL_USER_ID, info.st_uid);
            entry.fastInsert(KIO::UDSEntry::UDS_LOCAL_GROUP_ID, info.st_gid);
            if (S_ISLNK(info.st_mode)) {
                entry.fastInsert(KIO::UDSEntry::UDS_LINK_DEST, QFile::symLinkTarget(local));
            }
            listEntry(entry);
        }
        return KIO::WorkerResult::pass();
    }

private:
    static int connectToHelper()
    {
        const QByteArray path = QFile::encodeName(socketPath());
        sockaddr_un address{};
        if (path.size() >= int(sizeof(address.sun_path))) {
            return -1;
        }
        const int fd = ::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
        if (fd < 0) {
            return -1;
        }
        address.sun_family = AF_UNIX;
        memcpy(address.sun_path, path.constData(), size_t(path.size()));
        if (::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0) {
            ::close(fd);
            return -1;
        }
        return fd;
    }

    static QByteArray receiveAnswer(int fd)
    {
        QByteArray buffer(MESSAGE_MAX, Qt::Uninitialized);
        QElapsedTimer waited;
        waited.start();
        while (waited.elapsed() < ANSWER_MS) {
            pollfd poll{fd, POLLIN, 0};
            if (::poll(&poll, 1, int(ANSWER_MS - waited.elapsed())) <= 0) {
                break;
            }
            const ssize_t got = ::recv(fd, buffer.data(), size_t(buffer.size()), 0);
            if (got <= 0) {
                break;
            }
            QByteArray reply(buffer.constData(), got);
            if (reply.startsWith("result\t")) {
                return reply;
            }
        }
        return {};
    }
};

} // namespace

// KIO finds the worker by its metadata (the protocol) and starts it through kdemain.
class KIOPluginForMetaData : public QObject
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.kde.kio.worker.aurora-search" FILE "searchworker.json")
};

extern "C" Q_DECL_EXPORT int kdemain(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("kio_aurora_search"));
    if (argc != 4) {
        return -1;
    }
    SearchWorker worker(argv[2], argv[3]);
    worker.dispatchLoop();
    return 0;
}

#include "searchworker.moc"
