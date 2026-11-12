// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements gitsheets.h.

#include "gitsheets.h"

#include "core/gitcommands.h"
#include "core/gitmenu.h"
#include "glassfield.h"
#include "gitviews.h"
#include "style.h"

#include <KFormat>
#include <KMessageBox>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace fe {

namespace {

constexpr int FIELD_HEIGHT = 28;
constexpr int TITLE_PIXEL_SIZE = 15;
/// The commits a history lists: enough for months of work, few enough to list at once.
constexpr int HISTORY_LIMIT = 500;

/// The sheet's title and the line under it, as the other sheets start.
QLabel *addHeading(QVBoxLayout *layout, const QString &title, const QString &line, QWidget *sheet)
{
    auto *heading = new QLabel(title, sheet);
    heading->setFont(style::uiFont(TITLE_PIXEL_SIZE, QFont::DemiBold));
    layout->addWidget(heading);
    auto *under = new QLabel(line, sheet);
    QPalette colors = under->palette();
    colors.setColor(QPalette::WindowText, style::TEXT_SECONDARY);
    under->setPalette(colors);
    under->setWordWrap(true);
    layout->addWidget(under);
    return under;
}

QLabel *problemLabel(QWidget *sheet)
{
    auto *label = new QLabel(sheet);
    QPalette colors = label->palette();
    colors.setColor(QPalette::WindowText, style::GIT_DELETED);
    label->setPalette(colors);
    return label;
}

/// Shows the diff of entry in view: the file's own, or for a new folder the files that would join the repository.
void showEntryDiff(const git::Repository &repository, const git::StatusEntry &entry, bool hasHead, DiffView *view)
{
    QPointer<DiffView> target(view);
    if (entry.kind == git::StatusEntry::Kind::Untracked && entry.path.endsWith(QLatin1Char('/'))) {
        readGit(repository.workTree, {QStringLiteral("ls-files"), QStringLiteral("--others"), QStringLiteral("--exclude-standard"), QStringLiteral("--"), entry.path}, view,
                [target](const GitOutput &files) {
                    if (target) {
                        target->setPlainText(QStringLiteral("A new folder. These files would join the repository:\n\n") + QString::fromUtf8(files.out));
                    }
                });
        return;
    }
    readGit(repository.workTree, git::diffArguments(entry, hasHead), view, [target](const GitOutput &diff) {
        if (target) {
            target->setDiff(diff.out, diff.err.isEmpty() ? QStringLiteral("No changes to show") : QString::fromUtf8(diff.err));
        }
    });
}

QPushButton *addButton(QDialogButtonBox *buttons, const QString &text, const char *iconName, QDialogButtonBox::ButtonRole role)
{
    QPushButton *button = buttons->addButton(text, role);
    if (iconName) {
        button->setIcon(QIcon::fromTheme(QString::fromLatin1(iconName)));
    }
    return button;
}

/// A message as git keeps it in MERGE_MSG: its comment lines left out.
QString withoutComments(const QString &text)
{
    QStringList kept;
    for (const QString &line : text.split(QLatin1Char('\n'))) {
        if (!line.startsWith(QLatin1Char('#'))) {
            kept.append(line);
        }
    }
    return kept.join(QLatin1Char('\n')).trimmed();
}

/// Splits a whole commit message into its summary line and the rest.
std::pair<QString, QString> splitMessage(const QString &message)
{
    const QString trimmed = message.trimmed();
    const qsizetype newline = trimmed.indexOf(QLatin1Char('\n'));
    if (newline < 0) {
        return {trimmed, QString()};
    }
    return {trimmed.left(newline).trimmed(), trimmed.mid(newline + 1).trimmed()};
}

} // namespace

CommitSheet::CommitSheet(const git::Repository &repository, const GitSnapshot &snapshot, GitChanged changed, QWidget *parent)
    : QDialog(parent)
    , m_repository(repository)
    , m_snapshot(snapshot)
    , m_changed(std::move(changed))
    , m_changes(new ChangesList(true, this))
    , m_diff(new DiffView(this))
    , m_summary(new GlassLineEdit(QString(), FIELD_HEIGHT, this))
    , m_description(new QPlainTextEdit(this))
    , m_amend(new QCheckBox(QStringLiteral("Amend the last commit"), this))
{
    const git::Status &status = snapshot.state.status;
    const QString branch = status.branch.isEmpty() ? QStringLiteral("a detached HEAD") : status.branch;
    setWindowTitle(QStringLiteral("Commit to %1").arg(branch));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(980, 640);
    auto *layout = new QVBoxLayout(this);
    addHeading(layout, QStringLiteral("Commit to %1").arg(branch),
               QStringLiteral("%1: %2").arg(QFileInfo(repository.workTree).fileName(), git::stateLine(snapshot.state)), this);
    auto *split = new QSplitter(Qt::Horizontal, this);
    split->addWidget(m_changes);
    split->addWidget(m_diff);
    split->setStretchFactor(1, 1);
    split->setSizes({320, 640});
    layout->addWidget(split, 1);
    m_summary->setPlaceholderText(QStringLiteral("Summary (required)"));
    m_summary->setAccessibleName(QStringLiteral("Summary"));
    layout->addWidget(m_summary);
    m_description->setPlaceholderText(QStringLiteral("Description"));
    m_description->setAccessibleName(QStringLiteral("Description"));
    m_description->setFixedHeight(84);
    layout->addWidget(m_description);
    auto *row = new QHBoxLayout;
    row->addWidget(m_amend);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_commitAndPush = addButton(buttons, status.upstream.isEmpty() ? QStringLiteral("Commit and Publish") : QStringLiteral("Commit and Push"), "vcs-push",
                                QDialogButtonBox::ActionRole);
    m_commit = addButton(buttons, QStringLiteral("Commit to %1").arg(branch), "vcs-commit", QDialogButtonBox::AcceptRole);
    m_commit->setDefault(true);
    row->addWidget(buttons, 1);
    layout->addLayout(row);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(m_commit, &QPushButton::clicked, this, [this] { commit(false); });
    connect(m_commitAndPush, &QPushButton::clicked, this, [this] { commit(true); });
    connect(m_changes, &ChangesList::currentEntryChanged, this, &CommitSheet::showDiffOfCurrent);
    connect(m_changes, &ChangesList::checksChanged, this, &CommitSheet::updateButtons);
    connect(m_summary, &QLineEdit::textChanged, this, &CommitSheet::updateButtons);
    connect(m_amend, &QCheckBox::toggled, this, [this](bool on) {
        updateButtons();
        if (!on || !m_summary->text().isEmpty()) {
            return;
        }
        QPointer<CommitSheet> sheet(this);
        readGit(m_repository.workTree, {QStringLiteral("log"), QStringLiteral("-1"), QStringLiteral("--format=%B")}, this, [sheet](const GitOutput &last) {
            if (sheet && sheet->m_summary->text().isEmpty()) {
                const auto [summary, body] = splitMessage(QString::fromUtf8(last.out));
                sheet->m_summary->setText(summary);
                sheet->m_description->setPlainText(body);
            }
        });
    });
    m_amend->setEnabled(!status.head.isEmpty() && snapshot.state.operation == git::Operation::None);
    // Pushing needs a branch on a remote; publishing makes one.
    m_commitAndPush->setVisible(!snapshot.state.remotes.isEmpty() && !status.branch.isEmpty());
    m_changes->setEntries(status.entries);
    if (snapshot.state.operation == git::Operation::Merge) {
        // A merge commits everything; git has the message ready.
        m_changes->setAllChecked(true, true);
        QFile message(repository.gitDir + QStringLiteral("/MERGE_MSG"));
        if (message.open(QIODevice::ReadOnly)) {
            const auto [summary, body] = splitMessage(withoutComments(QString::fromUtf8(message.readAll())));
            m_summary->setText(summary);
            m_description->setPlainText(body);
        }
    }
    m_summary->setFocus();
    updateButtons();
}

ChangesList *CommitSheet::changes() const
{
    return m_changes;
}

QLineEdit *CommitSheet::summary() const
{
    return m_summary;
}

QPlainTextEdit *CommitSheet::description() const
{
    return m_description;
}

QCheckBox *CommitSheet::amend() const
{
    return m_amend;
}

QPushButton *CommitSheet::commitButton() const
{
    return m_commit;
}

QPushButton *CommitSheet::commitAndPushButton() const
{
    return m_commitAndPush;
}

DiffView *CommitSheet::diff() const
{
    return m_diff;
}

void CommitSheet::showDiffOfCurrent()
{
    if (const std::optional<git::StatusEntry> entry = m_changes->currentEntry()) {
        showEntryDiff(m_repository, *entry, !m_snapshot.state.status.head.isEmpty(), m_diff);
    } else {
        m_diff->setDiff({}, QStringLiteral("Nothing has changed since the last commit"));
    }
}

void CommitSheet::updateButtons()
{
    const bool something = !m_changes->checkedEntries().isEmpty() || m_amend->isChecked();
    const bool ready = !m_busy && something && !m_summary->text().trimmed().isEmpty();
    m_commit->setEnabled(ready);
    m_commitAndPush->setEnabled(ready);
}

void CommitSheet::commit(bool push)
{
    const git::Status &status = m_snapshot.state.status;
    QList<QStringList> steps = git::commitSteps(m_changes->checkedEntries(), m_summary->text(), m_description->toPlainText(), m_amend->isChecked(),
                                                m_snapshot.state.operation == git::Operation::Merge);
    QString title = QStringLiteral("Committing to %1").arg(status.branch);
    if (push) {
        const QList<git::Remote> &remotes = m_snapshot.state.remotes;
        QString remote = remotes.isEmpty() ? QString() : remotes.constFirst().name;
        for (const git::Remote &candidate : remotes) {
            if (candidate.name == u"origin") {
                remote = candidate.name;
            }
        }
        steps.append(status.upstream.isEmpty() ? git::publishArguments(remote) : QStringList{QStringLiteral("push"), QStringLiteral("--progress")});
        title = QStringLiteral("Committing to %1 and pushing").arg(status.branch);
    }
    m_busy = true;
    updateButtons();
    QPointer<CommitSheet> sheet(this);
    const GitChanged changed = m_changed;
    startGitJob(new GitJob(m_repository.workTree, steps, title), this, [sheet, changed](bool ok) {
        if (changed) {
            changed();
        }
        if (!sheet) {
            return;
        }
        sheet->m_busy = false;
        if (ok) {
            sheet->accept();
        } else {
            sheet->updateButtons();
        }
    });
}

ChangesSheet::ChangesSheet(const git::Repository &repository, const GitSnapshot &snapshot, const QStringList &paths, QWidget *parent)
    : QDialog(parent)
    , m_repository(repository)
    , m_hasHead(!snapshot.state.status.head.isEmpty())
    , m_changes(new ChangesList(false, this))
    , m_diff(new DiffView(this))
{
    const QString where = paths.size() == 1 ? QFileInfo(paths.constFirst()).fileName() : QFileInfo(repository.workTree).fileName();
    setWindowTitle(QStringLiteral("Changes in %1").arg(where));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(980, 600);
    auto *layout = new QVBoxLayout(this);
    QList<git::StatusEntry> entries;
    if (paths.isEmpty()) {
        entries = snapshot.state.status.entries;
    } else {
        for (const QString &path : paths) {
            for (const git::StatusEntry &entry : git::entriesUnder(snapshot.state.status, path)) {
                if (std::none_of(entries.cbegin(), entries.cend(), [&entry](const git::StatusEntry &seen) { return seen.path == entry.path; })) {
                    entries.append(entry);
                }
            }
        }
    }
    addHeading(layout, QStringLiteral("Changes in %1").arg(where), git::stateLine(snapshot.state), this);
    auto *split = new QSplitter(Qt::Horizontal, this);
    split->addWidget(m_changes);
    split->addWidget(m_diff);
    split->setStretchFactor(1, 1);
    split->setSizes({320, 640});
    layout->addWidget(split, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    connect(m_changes, &ChangesList::currentEntryChanged, this, [this] {
        if (const std::optional<git::StatusEntry> entry = m_changes->currentEntry()) {
            showEntryDiff(m_repository, *entry, m_hasHead, m_diff);
        }
    });
    m_changes->setEntries(entries);
    if (m_changes->entries().isEmpty()) {
        m_diff->setDiff({}, QStringLiteral("Nothing has changed since the last commit"));
    }
}

ChangesList *ChangesSheet::changes() const
{
    return m_changes;
}

DiffView *ChangesSheet::diff() const
{
    return m_diff;
}

CloneSheet::CloneSheet(QString folder, GitChanged changed, QWidget *parent)
    : QDialog(parent)
    , m_folder(std::move(folder))
    , m_changed(std::move(changed))
    , m_address(new GlassLineEdit(QStringLiteral("globe"), FIELD_HEIGHT, this))
    , m_name(new GlassLineEdit(QStringLiteral("folder"), FIELD_HEIGHT, this))
    , m_problem(problemLabel(this))
{
    setWindowTitle(QStringLiteral("Clone a Repository"));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(560, 0);
    auto *layout = new QVBoxLayout(this);
    addHeading(layout, QStringLiteral("Clone a Repository"), QStringLiteral("Into %1").arg(m_folder), this);
    m_address->setPlaceholderText(QStringLiteral("https://github.com/owner/repository.git"));
    m_address->setAccessibleName(QStringLiteral("Repository address"));
    m_name->setPlaceholderText(QStringLiteral("Folder name"));
    m_name->setAccessibleName(QStringLiteral("Folder name"));
    layout->addWidget(m_address);
    layout->addWidget(m_name);
    layout->addWidget(m_problem);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_clone = addButton(buttons, QStringLiteral("Clone"), "vcs-pull", QDialogButtonBox::AcceptRole);
    m_clone->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    connect(m_address, &QLineEdit::textChanged, this, [this](const QString &address) {
        if (!m_nameEdited) {
            const QSignalBlocker blocker(m_name);
            m_name->setText(address.trimmed().isEmpty() ? QString() : git::cloneFolderName(address));
        }
        updateButton();
    });
    connect(m_name, &QLineEdit::textEdited, this, [this] {
        m_nameEdited = !m_name->text().isEmpty();
        updateButton();
    });
    // An address copied a moment ago is most likely the one to clone.
    const QString copied = QApplication::clipboard()->text().trimmed();
    if (!copied.contains(QLatin1Char('\n')) && !git::remoteWebPage(copied).isEmpty()) {
        m_address->setText(copied);
    }
    m_address->setFocus();
    updateButton();
}

QLineEdit *CloneSheet::address() const
{
    return m_address;
}

QLineEdit *CloneSheet::name() const
{
    return m_name;
}

QPushButton *CloneSheet::cloneButton() const
{
    return m_clone;
}

void CloneSheet::updateButton()
{
    const QString name = m_name->text().trimmed();
    QString problem;
    if (name.contains(QLatin1Char('/')) || name == u"." || name == u"..") {
        problem = QStringLiteral("The folder name cannot hold a slash.");
    } else if (!name.isEmpty() && QFileInfo::exists(m_folder + QLatin1Char('/') + name)) {
        problem = QStringLiteral("%1 is already here; choose another name.").arg(name);
    }
    m_problem->setText(problem);
    m_problem->setVisible(!problem.isEmpty());
    m_clone->setEnabled(problem.isEmpty() && !name.isEmpty() && !m_address->text().trimmed().isEmpty());
}

void CloneSheet::accept()
{
    if (!m_clone->isEnabled()) {
        return;
    }
    const QString name = m_name->text().trimmed();
    auto *job = new GitJob(m_folder, {{QStringLiteral("clone"), QStringLiteral("--progress"), QStringLiteral("--"), m_address->text().trimmed(), name}},
                           QStringLiteral("Cloning %1").arg(name));
    const GitChanged changed = m_changed;
    // The transfer center shows the clone from here on; the sheet's work is done.
    startGitJob(job, parentWidget(), [changed](bool) {
        if (changed) {
            changed();
        }
    });
    QDialog::accept();
}

BranchesSheet::BranchesSheet(const git::Repository &repository, GitChanged changed, QWidget *parent)
    : QDialog(parent)
    , m_repository(repository)
    , m_changed(std::move(changed))
    , m_filter(new GlassLineEdit(QStringLiteral("search"), FIELD_HEIGHT, this))
    , m_list(new QTreeWidget(this))
    , m_newName(new GlassLineEdit(QStringLiteral("vcs-branch"), FIELD_HEIGHT, this))
    , m_problem(problemLabel(this))
{
    setWindowTitle(QStringLiteral("Branches of %1").arg(QFileInfo(repository.workTree).fileName()));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(640, 560);
    auto *layout = new QVBoxLayout(this);
    m_where = addHeading(layout, QStringLiteral("Branches"), QFileInfo(repository.workTree).fileName(), this);
    m_filter->setPlaceholderText(QStringLiteral("Filter"));
    m_filter->setAccessibleName(QStringLiteral("Filter branches"));
    layout->addWidget(m_filter);
    m_list->setColumnCount(3);
    m_list->setHeaderLabels({QStringLiteral("Branch"), QStringLiteral("State"), QStringLiteral("Last commit")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setAccessibleName(QStringLiteral("Branches"));
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_list->header()->setStretchLastSection(false);
    layout->addWidget(m_list, 1);
    auto *create = new QHBoxLayout;
    m_newName->setPlaceholderText(QStringLiteral("New branch name"));
    m_newName->setAccessibleName(QStringLiteral("New branch name"));
    create->addWidget(m_newName, 1);
    m_create = new QPushButton(QIcon::fromTheme(QStringLiteral("list-add")), QStringLiteral("Create Branch"), this);
    create->addWidget(m_create);
    layout->addLayout(create);
    layout->addWidget(m_problem);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_delete = addButton(buttons, QStringLiteral("Delete"), "edit-delete", QDialogButtonBox::ActionRole);
    m_merge = addButton(buttons, QStringLiteral("Merge into Current"), "vcs-merge", QDialogButtonBox::ActionRole);
    m_switch = addButton(buttons, QStringLiteral("Switch"), "vcs-branch", QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &text) {
        for (int i = 0; i < m_list->topLevelItemCount(); ++i) {
            QTreeWidgetItem *row = m_list->topLevelItem(i);
            row->setHidden(!row->text(0).contains(text.trimmed(), Qt::CaseInsensitive));
        }
    });
    connect(m_list, &QTreeWidget::currentItemChanged, this, &BranchesSheet::updateButtons);
    connect(m_list, &QTreeWidget::itemDoubleClicked, this, [this] { m_switch->click(); });
    connect(m_newName, &QLineEdit::textChanged, this, &BranchesSheet::updateButtons);
    connect(m_newName, &QLineEdit::returnPressed, m_create, &QPushButton::click);
    connect(m_create, &QPushButton::clicked, this, [this] {
        const QString name = m_newName->text().trimmed();
        run({{QStringLiteral("switch"), QStringLiteral("-c"), name}}, QStringLiteral("Creating branch %1").arg(name));
        m_newName->clear();
    });
    connect(m_switch, &QPushButton::clicked, this, [this] {
        if (const std::optional<git::Branch> branch = selected(); branch && !branch->current) {
            run({git::switchArguments(*branch)}, QStringLiteral("Switching to %1").arg(branch->name));
        }
    });
    connect(m_merge, &QPushButton::clicked, this, [this] {
        if (const std::optional<git::Branch> branch = selected(); branch && !branch->current) {
            run({{QStringLiteral("merge"), QStringLiteral("--no-edit"), branch->name}}, QStringLiteral("Merging %1").arg(branch->name));
        }
    });
    connect(m_delete, &QPushButton::clicked, this, [this] {
        const std::optional<git::Branch> branch = selected();
        if (!branch || branch->current || branch->remote) {
            return;
        }
        // git deletes only a branch whose commits are merged; one that is not stays, and git says so.
        if (KMessageBox::warningContinueCancel(this, QStringLiteral("Delete the branch %1?").arg(branch->name), QStringLiteral("Delete Branch"),
                                               KGuiItem(QStringLiteral("Delete"), QStringLiteral("edit-delete")))
            == KMessageBox::Continue) {
            run({{QStringLiteral("branch"), QStringLiteral("-d"), QStringLiteral("--"), branch->name}}, QStringLiteral("Deleting branch %1").arg(branch->name));
        }
    });
    reload();
    updateButtons();
}

QTreeWidget *BranchesSheet::list() const
{
    return m_list;
}

QLineEdit *BranchesSheet::newName() const
{
    return m_newName;
}

QPushButton *BranchesSheet::createButton() const
{
    return m_create;
}

QPushButton *BranchesSheet::switchButton() const
{
    return m_switch;
}

void BranchesSheet::reload()
{
    QPointer<BranchesSheet> sheet(this);
    readGit(m_repository.workTree, {QStringLiteral("for-each-ref"), QStringLiteral("--format=") + git::branchFormat(), QStringLiteral("refs/heads"), QStringLiteral("refs/remotes")},
            this, [sheet](const GitOutput &refs) {
                if (!sheet) {
                    return;
                }
                sheet->m_branches = git::parseBranches(refs.out);
                sheet->m_list->clear();
                const KFormat format;
                QFont bold = sheet->m_list->font();
                bold.setBold(true);
                for (qsizetype i = 0; i < sheet->m_branches.size(); ++i) {
                    const git::Branch &branch = sheet->m_branches.at(i);
                    auto *row = new QTreeWidgetItem(sheet->m_list);
                    row->setText(0, branch.name);
                    row->setIcon(0, QIcon::fromTheme(branch.remote ? QStringLiteral("folder-cloud") : QStringLiteral("vcs-branch")));
                    row->setText(1, branch.current ? (branch.track.isEmpty() ? QStringLiteral("Current") : QStringLiteral("Current, %1").arg(branch.track)) : branch.track);
                    row->setText(2, format.formatRelativeDateTime(branch.time, QLocale::ShortFormat));
                    row->setData(0, Qt::UserRole, i);
                    if (branch.current) {
                        row->setFont(0, bold);
                        row->setForeground(0, style::ACCENT);
                        sheet->m_where->setText(QStringLiteral("On %1").arg(branch.name));
                    }
                }
                sheet->updateButtons();
                Q_EMIT sheet->finished();
            });
}

std::optional<git::Branch> BranchesSheet::selected() const
{
    if (const QTreeWidgetItem *row = m_list->currentItem()) {
        return m_branches.value(row->data(0, Qt::UserRole).toInt());
    }
    return std::nullopt;
}

void BranchesSheet::updateButtons()
{
    const std::optional<git::Branch> branch = selected();
    const bool other = branch && !branch->current;
    m_switch->setEnabled(other);
    m_merge->setEnabled(other);
    m_delete->setEnabled(other && !branch->remote);
    const QString name = m_newName->text().trimmed();
    const QString problem = name.isEmpty() ? QString() : git::branchNameProblem(name);
    const bool taken = std::any_of(m_branches.cbegin(), m_branches.cend(), [&name](const git::Branch &known) { return !known.remote && known.name == name; });
    m_problem->setText(taken ? QStringLiteral("There is a branch called %1 already.").arg(name) : problem);
    m_problem->setVisible(!m_problem->text().isEmpty());
    m_create->setEnabled(!name.isEmpty() && problem.isEmpty() && !taken);
}

void BranchesSheet::run(const QList<QStringList> &steps, const QString &title)
{
    QPointer<BranchesSheet> sheet(this);
    const GitChanged changed = m_changed;
    startGitJob(new GitJob(m_repository.workTree, steps, title), this, [sheet, changed](bool) {
        if (changed) {
            changed();
        }
        if (sheet) {
            sheet->reload();
        }
    });
}

HistorySheet::HistorySheet(const git::Repository &repository, const QStringList &paths, QWidget *parent)
    : QDialog(parent)
    , m_repository(repository)
    , m_paths(paths)
    , m_list(new QTreeWidget(this))
    , m_diff(new DiffView(this))
{
    const QString what = paths.size() == 1 ? QFileInfo(paths.constFirst()).fileName() : QFileInfo(repository.workTree).fileName();
    setWindowTitle(QStringLiteral("History of %1").arg(what));
    setAttribute(Qt::WA_DeleteOnClose);
    resize(980, 680);
    auto *layout = new QVBoxLayout(this);
    QLabel *under = addHeading(layout, QStringLiteral("History of %1").arg(what), QStringLiteral("Reading the commits"), this);
    m_list->setColumnCount(4);
    m_list->setHeaderLabels({QStringLiteral("Commit"), QStringLiteral("Author"), QStringLiteral("Date"), QStringLiteral("Id")});
    m_list->setRootIsDecorated(false);
    m_list->setUniformRowHeights(true);
    m_list->setFrameShape(QFrame::NoFrame);
    m_list->setAccessibleName(QStringLiteral("Commits"));
    m_list->header()->setStretchLastSection(false);
    m_list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int column = 1; column < 4; ++column) {
        m_list->header()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    }
    auto *split = new QSplitter(Qt::Vertical, this);
    split->addWidget(m_list);
    split->addWidget(m_diff);
    split->setSizes({260, 380});
    layout->addWidget(split, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    QPushButton *copy = addButton(buttons, QStringLiteral("Copy Id"), "edit-copy", QDialogButtonBox::ActionRole);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
    connect(copy, &QPushButton::clicked, this, [this] {
        if (const QTreeWidgetItem *row = m_list->currentItem()) {
            QApplication::clipboard()->setText(m_commits.value(row->data(0, Qt::UserRole).toInt()).hash);
        }
    });
    connect(m_list, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *row) {
        if (!row) {
            return;
        }
        const QString hash = m_commits.value(row->data(0, Qt::UserRole).toInt()).hash;
        QPointer<DiffView> view(m_diff);
        readGit(m_repository.workTree,
                QStringList{QStringLiteral("show"), QStringLiteral("--stat"), QStringLiteral("--patch"), QStringLiteral("--format=fuller"), hash, QStringLiteral("--")} + m_paths, this,
                [view](const GitOutput &shown) {
                    if (view) {
                        view->setDiff(shown.out);
                    }
                });
    });
    QPointer<HistorySheet> sheet(this);
    QPointer<QLabel> line(under);
    readGit(repository.workTree, git::logArguments(paths, HISTORY_LIMIT), this, [sheet, line](const GitOutput &log) {
        if (!sheet) {
            return;
        }
        sheet->m_commits = git::parseLog(log.out);
        const KFormat format;
        for (qsizetype i = 0; i < sheet->m_commits.size(); ++i) {
            const git::Commit &commit = sheet->m_commits.at(i);
            auto *row = new QTreeWidgetItem(sheet->m_list, {commit.subject, commit.author, format.formatRelativeDateTime(commit.time, QLocale::ShortFormat), commit.shortHash});
            row->setToolTip(0, commit.subject);
            row->setToolTip(1, commit.email);
            row->setData(0, Qt::UserRole, i);
        }
        if (line) {
            line->setText(sheet->m_commits.isEmpty() ? QStringLiteral("No commits yet")
                          : sheet->m_commits.size() >= HISTORY_LIMIT ? QStringLiteral("The latest %1 commits").arg(HISTORY_LIMIT)
                                                                      : QStringLiteral("%1 commits").arg(sheet->m_commits.size()));
        }
        if (sheet->m_list->topLevelItemCount() > 0) {
            sheet->m_list->setCurrentItem(sheet->m_list->topLevelItem(0));
        }
        Q_EMIT sheet->loaded();
    });
}

QTreeWidget *HistorySheet::list() const
{
    return m_list;
}

DiffView *HistorySheet::diff() const
{
    return m_diff;
}

} // namespace fe
