// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements gitrunner.h.

#include "gitrunner.h"

#include <KIO/JobTracker>
#include <KJobTrackerInterface>
#include <KMessageBox>

#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QWidget>

#include <memory>

namespace fe {

namespace {

/// The lines of git's error output that a message shows: its last ones, where git says what went wrong.
constexpr int ERROR_LINES = 12;

QString lastLines(const QByteArray &text)
{
    QStringList lines = QString::fromUtf8(text).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    // Progress lines end in carriage returns; only their last state is worth reading.
    for (QString &line : lines) {
        line = line.section(QLatin1Char('\r'), -1).trimmed();
    }
    lines.removeAll(QString());
    return lines.mid(qMax(0, int(lines.size()) - ERROR_LINES)).join(QLatin1Char('\n'));
}

} // namespace

QProcessEnvironment gitEnvironment()
{
    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    environment.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    // git and ssh ask for passwords through an askpass program instead of a terminal (gitcredentials(7), ssh(1)).
    const QString askpass = QStandardPaths::findExecutable(QStringLiteral("ksshaskpass"));
    if (!askpass.isEmpty()) {
        if (!environment.contains(QStringLiteral("GIT_ASKPASS"))) {
            environment.insert(QStringLiteral("GIT_ASKPASS"), askpass);
        }
        if (!environment.contains(QStringLiteral("SSH_ASKPASS"))) {
            environment.insert(QStringLiteral("SSH_ASKPASS"), askpass);
        }
        environment.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("prefer"));
    }
    // A merge or rebase that would open an editor takes git's own message instead.
    environment.insert(QStringLiteral("GIT_EDITOR"), QStringLiteral("true"));
    environment.insert(QStringLiteral("GIT_MERGE_AUTOEDIT"), QStringLiteral("no"));
    environment.insert(QStringLiteral("LC_MESSAGES"), QStringLiteral("C"));
    environment.remove(QStringLiteral("LANGUAGE"));
    return environment;
}

QStringList gitSafeDirectories()
{
    QProcess process;
    process.setProcessEnvironment(gitEnvironment());
    // From a folder that is no repository, git reads only the system and user settings, the ones safe.directory
    // counts in.
    process.setWorkingDirectory(QStringLiteral("/"));
    process.start(QStringLiteral("git"), {QStringLiteral("config"), QStringLiteral("--get-all"), QStringLiteral("safe.directory")});
    if (!process.waitForFinished(1000)) {
        process.kill();
        return {};
    }
    return QString::fromUtf8(process.readAllStandardOutput()).split(QLatin1Char('\n'));
}

void readGit(const QString &folder, const QStringList &arguments, QObject *context, std::function<void(const GitOutput &)> done)
{
    auto *process = new QProcess(context);
    process->setProcessEnvironment(gitEnvironment());
    process->setWorkingDirectory(folder);
    QObject::connect(process, &QProcess::finished, context, [process, done](int code, QProcess::ExitStatus status) {
        GitOutput output{status == QProcess::NormalExit ? code : -1, process->readAllStandardOutput(), process->readAllStandardError()};
        process->deleteLater();
        done(output);
    });
    QObject::connect(process, &QProcess::errorOccurred, context, [process, done](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            process->deleteLater();
            done({-1, {}, QByteArrayLiteral("git could not be started")});
        }
    });
    process->start(QStringLiteral("git"), arguments);
}

void loadGitSnapshot(const git::Repository &repository, const QStringList &paths, QObject *context,
                     std::function<void(const std::optional<GitSnapshot> &)> done)
{
    const QString folder = repository.workTree;
    auto snapshot = std::make_shared<GitSnapshot>();
    snapshot->state.operation = git::operationInProgress(repository.gitDir);
    for (const QString &path : paths) {
        snapshot->items.append({path, false, false});
    }
    const auto finish = [snapshot, done] { done(*snapshot); };
    // Each read starts the next one, so they share nothing but the snapshot and stop with context.
    const auto readIgnored = [folder, paths, context, snapshot, finish] {
        if (paths.isEmpty()) {
            finish();
            return;
        }
        readGit(folder, QStringList{QStringLiteral("check-ignore"), QStringLiteral("-z"), QStringLiteral("--")} + paths, context,
                [snapshot, finish](const GitOutput &ignored) {
                    for (const QByteArray &path : ignored.out.split('\0')) {
                        for (git::ItemFacts &item : snapshot->items) {
                            if (item.path == QString::fromUtf8(path)) {
                                item.ignored = true;
                            }
                        }
                    }
                    finish();
                });
    };
    const auto readTracked = [folder, paths, context, snapshot, readIgnored] {
        if (paths.isEmpty()) {
            readIgnored();
            return;
        }
        readGit(folder, QStringList{QStringLiteral("ls-files"), QStringLiteral("-z"), QStringLiteral("--")} + paths, context,
                [snapshot, readIgnored](const GitOutput &tracked) {
                    for (const QByteArray &raw : tracked.out.split('\0')) {
                        const QString file = QString::fromUtf8(raw);
                        for (git::ItemFacts &item : snapshot->items) {
                            if (!file.isEmpty() && (file == item.path || file.startsWith(item.path + QLatin1Char('/')))) {
                                item.tracked = true;
                            }
                        }
                    }
                    readIgnored();
                });
    };
    readGit(folder, {QStringLiteral("status"), QStringLiteral("--porcelain=v2"), QStringLiteral("--branch"), QStringLiteral("--show-stash"), QStringLiteral("-z")},
            context, [folder, context, snapshot, readTracked, done](const GitOutput &status) {
                std::optional<git::Status> parsed = status.exitCode == 0 ? git::parseStatus(status.out) : std::nullopt;
                if (!parsed) {
                    done(std::nullopt);
                    return;
                }
                snapshot->state.status = *parsed;
                readGit(folder, {QStringLiteral("remote"), QStringLiteral("-v")}, context, [snapshot, readTracked](const GitOutput &remotes) {
                    snapshot->state.remotes = git::parseRemotes(remotes.out);
                    readTracked();
                });
            });
}

GitJob::GitJob(QString folder, QList<QStringList> steps, QString title, QObject *parent)
    : KJob(parent)
    , m_folder(std::move(folder))
    , m_steps(std::move(steps))
    , m_title(std::move(title))
{
}

GitJob::~GitJob()
{
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
        m_process->waitForFinished(2000);
    }
}

void GitJob::start()
{
    Q_EMIT description(this, m_title);
    runNext();
}

QString GitJob::title() const
{
    return m_title;
}

QByteArray GitJob::output() const
{
    return m_output;
}

bool GitJob::doKill()
{
    if (m_process) {
        m_process->kill();
    }
    return true;
}

void GitJob::runNext()
{
    if (m_next >= m_steps.size()) {
        setPercent(100);
        emitResult();
        return;
    }
    const QStringList arguments = m_steps.at(m_next++);
    if (m_process) {
        // This runs from the last step's finished signal, so that process can only go once it has returned.
        m_process->disconnect(this);
        m_process->deleteLater();
    }
    m_process = new QProcess(this);
    m_process->setProcessEnvironment(gitEnvironment());
    m_process->setWorkingDirectory(m_folder);
    m_errors.clear();
    connect(m_process, &QProcess::readyReadStandardError, this, &GitJob::readProgress);
    connect(m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_output = m_process->readAllStandardOutput();
        readProgress();
        if (status != QProcess::NormalExit || code != 0) {
            setError(KJob::UserDefinedError);
            const QString message = lastLines(m_errors);
            setErrorText(message.isEmpty() ? QStringLiteral("git stopped with code %1.").arg(code) : message);
            emitResult();
            return;
        }
        runNext();
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            setError(KJob::UserDefinedError);
            setErrorText(QStringLiteral("git could not be started. Is it installed?"));
            emitResult();
        }
    });
    m_process->start(QStringLiteral("git"), arguments);
}

void GitJob::readProgress()
{
    const QByteArray chunk = m_process->readAllStandardError();
    m_errors += chunk;
    // git's progress lines: "Receiving objects:  45% (450/1000)", each redrawn after a carriage return.
    static const QRegularExpression progress(QStringLiteral("([A-Z][a-z]+(?: [a-z]+)*):\\s+(\\d{1,3})%"));
    QRegularExpressionMatchIterator matches = progress.globalMatch(QString::fromUtf8(chunk));
    QRegularExpressionMatch last;
    while (matches.hasNext()) {
        last = matches.next();
    }
    if (last.hasMatch()) {
        setPercent(last.captured(2).toULong());
        Q_EMIT description(this, m_title, qMakePair(QStringLiteral("Step"), last.captured(1)));
    }
}

void startGitJob(GitJob *job, QWidget *window, std::function<void(bool)> done)
{
    QPointer<QWidget> parent(window);
    QObject::connect(job, &KJob::result, job, [job, parent, done](KJob *) {
        const bool ok = job->error() == 0;
        if (!ok && job->error() != KJob::KilledJobError && parent) {
            KMessageBox::detailedError(parent, QStringLiteral("%1 did not finish.").arg(job->title()), job->errorText(), job->title());
        }
        if (done) {
            done(ok);
        }
    });
    if (KJobTrackerInterface *tracker = KIO::getJobTracker()) {
        tracker->registerJob(job);
    }
    job->start();
}

} // namespace fe
