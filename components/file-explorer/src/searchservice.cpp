// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements searchservice.h. The protocol is the helper's (components/file-explorer/search/src/server.rs).

#include "searchservice.h"

#include <QElapsedTimer>
#include <QFile>
#include <QSocketNotifier>
#include <QTimer>

#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace fe {

namespace {

/// How long the connection stays without a window being used or a search, in milliseconds. While it is open the
/// helper follows changes live; after it closes the helper leaves after its own idle time.
constexpr int LET_GO_MS = 3 * 60 * 1000;
/// How long `covers` waits for the helper, in milliseconds: a started helper answers within a few, a fresh start
/// that loads its index within a few hundred.
constexpr int ANSWER_MS = 800;
constexpr int MESSAGE_MAX = 64 * 1024;

} // namespace

SearchService &SearchService::instance()
{
    static SearchService service;
    return service;
}

SearchService::SearchService()
    : m_letGo(new QTimer(this))
{
    m_letGo->setSingleShot(true);
    m_letGo->setInterval(LET_GO_MS);
    connect(m_letGo, &QTimer::timeout, this, &SearchService::disconnectNow);
}

SearchService::~SearchService()
{
    disconnectNow();
}

QString SearchService::socketPath()
{
    return qEnvironmentVariable("AURORA_SEARCH_SOCKET", QStringLiteral("/run/aurora/search.sock"));
}

void SearchService::warm()
{
    if (m_fd < 0) {
        connectNow();
    }
    m_letGo->start();
}

void SearchService::connectNow()
{
    const QByteArray path = QFile::encodeName(socketPath());
    sockaddr_un address{};
    if (path.size() >= int(sizeof(address.sun_path))) {
        return;
    }
    const int fd = ::socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC | SOCK_NONBLOCK, 0);
    if (fd < 0) {
        return;
    }
    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, path.constData(), size_t(path.size()));
    // A non-blocking connect to a Unix socket completes at once or fails at once; systemd queues it while it starts
    // the helper.
    if (::connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 && errno != EINPROGRESS) {
        ::close(fd);
        return;
    }
    m_fd = fd;
    static constexpr char HELLO[] = "hello";
    ::send(m_fd, HELLO, sizeof(HELLO) - 1, MSG_NOSIGNAL);
    m_notifier = new QSocketNotifier(m_fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated, this, &SearchService::readMessages);
}

void SearchService::disconnectNow()
{
    if (m_fd < 0) {
        return;
    }
    delete m_notifier;
    m_notifier = nullptr;
    ::close(m_fd);
    m_fd = -1;
}

void SearchService::readMessages()
{
    QByteArray buffer(MESSAGE_MAX, Qt::Uninitialized);
    for (;;) {
        const ssize_t got = ::recv(m_fd, buffer.data(), size_t(buffer.size()), MSG_DONTWAIT);
        if (got == 0 || (got < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            // The helper left (idle, or stopped): the next warm() starts it again.
            disconnectNow();
            return;
        }
        if (got < 0) {
            return;
        }
        if (QByteArrayView(buffer.constData(), got) == QByteArrayView("changed\n")) {
            Q_EMIT indexChanged();
        }
    }
}

bool SearchService::covers(const QUrl &folder)
{
    warm();
    if (m_fd < 0 || (!folder.isEmpty() && !folder.isLocalFile())) {
        return false;
    }
    const quint64 id = m_nextId++;
    // An empty query with a scope: the helper answers "not-indexed" for a folder it does not hold, and nothing else.
    QByteArray message = "query\t" + QByteArray::number(id) + "\t1\t0\t0\t";
    message += folder.isEmpty() ? QByteArray() : QFile::encodeName(folder.toLocalFile());
    message += '\t';
    if (::send(m_fd, message.constData(), size_t(message.size()), MSG_NOSIGNAL) < 0) {
        disconnectNow();
        return false;
    }
    const QByteArray expected = "result\t" + QByteArray::number(id) + '\t';
    QByteArray buffer(MESSAGE_MAX, Qt::Uninitialized);
    QElapsedTimer waited;
    waited.start();
    while (waited.elapsed() < ANSWER_MS) {
        pollfd poll{m_fd, POLLIN, 0};
        if (::poll(&poll, 1, int(ANSWER_MS - waited.elapsed())) <= 0) {
            break;
        }
        const ssize_t got = ::recv(m_fd, buffer.data(), size_t(buffer.size()), MSG_DONTWAIT);
        if (got <= 0) {
            if (got == 0) {
                disconnectNow();
            }
            break;
        }
        const QByteArray reply(buffer.constData(), got);
        if (reply == "changed\n") {
            Q_EMIT indexChanged();
            continue;
        }
        if (reply.startsWith(expected)) {
            // result, id, total, more, state, error
            const QList<QByteArray> fields = reply.left(reply.indexOf('\n')).split('\t');
            return fields.size() >= 6 && fields.at(5).isEmpty();
        }
    }
    return false;
}

} // namespace fe
