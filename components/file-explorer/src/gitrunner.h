// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Runs git for File Explorer: quick reads whose output a callback gets, and operations as jobs that the transfer
// center lists with their progress, in an environment where git never waits on a terminal.

#pragma once

#include "core/gitmenu.h"
#include "core/gitrepo.h"

#include <KJob>

#include <QByteArray>
#include <QList>
#include <QPointer>
#include <QProcessEnvironment>
#include <QStringList>

#include <functional>
#include <optional>

class QProcess;
class QWidget;

namespace fe {

/// The environment git runs in: the session's, with terminal prompts off, KDE's password prompt for HTTPS and SSH
/// passwords when it is installed, an editor that accepts git's default messages, and git's messages in English.
[[nodiscard]] QProcessEnvironment gitEnvironment();

/// The `safe.directory` entries of git's system and user settings (git-config(1)), read on the spot; only asked for
/// when a repository belongs to another user, so the wait (at most a second) is rare.
[[nodiscard]] QStringList gitSafeDirectories();

struct GitOutput {
    /// git's exit code; -1 when it could not run or crashed.
    int exitCode = -1;
    QByteArray out;
    QByteArray err;
};

/// Runs git with arguments in folder and calls done on the GUI thread with what it printed. The run belongs to
/// context: when context goes away first, git is stopped and done is never called.
void readGit(const QString &folder, const QStringList &arguments, QObject *context, std::function<void(const GitOutput &)> done);

/// What the Git menu and the sheets know of a repository at one moment.
struct GitSnapshot {
    git::RepositoryState state;
    /// The facts of each path asked for, in the same order.
    QList<git::ItemFacts> items;
};

/// Reads a repository's status, remotes and the operation under way, and for each of paths (relative to the work
/// tree's top) whether git tracks and ignores it; done gets nothing when git could not read the repository.
void loadGitSnapshot(const git::Repository &repository, const QStringList &paths, QObject *context,
                     std::function<void(const std::optional<GitSnapshot> &)> done);

/// Runs git commands one after the other in a folder, as one job: the first that fails ends it with git's message.
/// Clone, fetch, pull and push report git's progress as a percent.
class GitJob : public KJob
{
    Q_OBJECT

public:
    /// title names the job in the transfer center ("Pushing to origin"); steps are git's arguments, without "git".
    GitJob(QString folder, QList<QStringList> steps, QString title, QObject *parent = nullptr);
    ~GitJob() override;

    void start() override;
    [[nodiscard]] QString title() const;
    /// What the last step printed on its standard output.
    [[nodiscard]] QByteArray output() const;

protected:
    bool doKill() override;

private:
    void runNext();
    void readProgress();

    QString m_folder;
    QList<QStringList> m_steps;
    QString m_title;
    qsizetype m_next = 0;
    QProcess *m_process = nullptr;
    QByteArray m_output;
    QByteArray m_errors;
};

/// Starts job in the transfer center and, when it fails, shows git's message over window. done, when given, is called
/// with whether it succeeded, after any message is closed.
void startGitJob(GitJob *job, QWidget *window, std::function<void(bool)> done = {});

} // namespace fe
