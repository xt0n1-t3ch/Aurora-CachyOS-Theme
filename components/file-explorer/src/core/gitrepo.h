// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Which Git repository holds a file or folder, found the way git itself finds it, so File Explorer offers Git only
// where git would work and always for the right repository.

#pragma once

#include <QString>
#include <QStringList>

#include <functional>
#include <optional>

namespace fe::git {

/// A repository with a work tree, as git would use it from a folder inside it.
struct Repository {
    /// The top folder of the work tree, with symbolic links resolved, as `git rev-parse --show-toplevel` prints it.
    QString workTree;
    /// The folder of this work tree's own Git files: `.git`, or what a `.git` file names for a linked work tree or a
    /// submodule.
    QString gitDir;
    /// The folder of the objects and refs that every work tree of the repository shares: gitDir, or what its
    /// `commondir` file names.
    QString commonDir;

    friend bool operator==(const Repository &, const Repository &) = default;
};

/// Whether path is a Git directory by git's own test (setup.c, is_git_directory, git 2.56): a HEAD that names a ref or
/// holds an object id, and `objects` and `refs` folders in it or in the folder its `commondir` file names.
[[nodiscard]] bool isGitDirectory(const QString &path);

/// Whether `safe.directory` entries (git-config(1)) let git use a repository owned by another user at workTree: `*`,
/// the same path, or a path ending in `/*` that workTree is inside. An empty entry forgets the entries before it.
[[nodiscard]] bool isSafeDirectory(const QString &workTree, const QStringList &entries);

/// The repository whose work tree holds path, by git's discovery (setup.c, setup_git_directory_gently_1): from the
/// folder itself (or a file's folder) up, the first `.git` folder that is a Git directory or `.git` file that names
/// one. A `.git` file that names nothing valid, a bare repository, a folder in GIT_CEILING_DIRECTORIES, another
/// filesystem on the way up (git stays on one unless GIT_DISCOVERY_ACROSS_FILESYSTEM is set) or a repository owned by
/// another user that safe.directory does not allow all end the search with nothing, as git ends it with an error.
/// safeDirectories is asked for the `safe.directory` entries only when the owner differs.
[[nodiscard]] std::optional<Repository> repositoryOf(const QString &path, bool isFolder,
                                                     const std::function<QStringList()> &safeDirectories = {});

/// A file or folder the Git menu is opened for.
struct SelectedPath {
    QString path;
    bool isFolder = false;
};

/// What the Git menu is for.
struct Target {
    enum class Kind {
        /// No Git menu: an item outside every repository, items of different repositories, or a folder that cannot
        /// hold a new one.
        None,
        /// The background of a folder outside every repository, which a repository can be cloned into or made in.
        Outside,
        /// A repository as a whole: the background of a folder inside its work tree, or the work tree's top folder.
        Repository,
        /// Files and folders inside one work tree.
        Items,
    };
    Kind kind = Kind::None;
    Repository repository;
    /// For Items: each path relative to the work tree's top, with "/" between folders.
    QStringList paths;
    /// For Outside: the folder to clone into or make a repository of.
    QString folder;
};

/// What the Git menu is for when it opens on items, or on the background of the folder items holds alone. A folder
/// item is taken where its links lead, as git takes the folder it starts in; a file stays where it is listed.
[[nodiscard]] Target targetOf(const QList<SelectedPath> &items, bool background, const std::function<QStringList()> &safeDirectories = {});

} // namespace fe::git
