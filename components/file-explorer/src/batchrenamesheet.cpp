// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements batchrenamesheet.h.

#include "batchrenamesheet.h"

#include <KIO/BatchRenameJob>
#include <KIO/FileUndoManager>
#include <KIO/JobUiDelegateFactory>
#include <KJobWidgets>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace fe {

namespace {

/// The folder key of an item: its folder's URL, equal for items of one folder.
QString folderKey(const QUrl &url)
{
    return url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash).toString();
}

constexpr int PREVIEW_MIN_HEIGHT = 220;
constexpr int SHEET_WIDTH = 620;
/// The red of a name that cannot be taken, the system red of the theme (style.h's CAPACITY_CRITICAL).
const QColor PROBLEM_RED{255, 69, 58};

} // namespace

BatchRenameSheet::BatchRenameSheet(const KFileItemList &items, QWidget *parent)
    : QDialog(parent)
    , m_items(items)
    , m_mode(new QComboBox(this))
    , m_pages(new QStackedWidget(this))
    , m_find(new QLineEdit(this))
    , m_replace(new QLineEdit(this))
    , m_matchCase(new QCheckBox(QStringLiteral("Match case"), this))
    , m_regex(new QCheckBox(QStringLiteral("Regular expression"), this))
    , m_addText(new QLineEdit(this))
    , m_addWhere(new QComboBox(this))
    , m_format(new QComboBox(this))
    , m_formatWhere(new QComboBox(this))
    , m_customName(new QLineEdit(this))
    , m_startAt(new QSpinBox(this))
    , m_case(new QComboBox(this))
    , m_preview(new QTreeWidget(this))
    , m_summary(new QLabel(this))
    , m_renameButton(nullptr)
{
    setWindowTitle(QStringLiteral("Rename %1 Items").arg(items.size()));
    setAttribute(Qt::WA_DeleteOnClose);
    for (const KFileItem &item : items) {
        m_renameItems.append({folderKey(item.url()), item.name(), item.time(KFileItem::ModificationTime)});
    }
    buildControls();
    readFolders();
    refresh();
    resize(SHEET_WIDTH, sizeHint().height());
}

void BatchRenameSheet::buildControls()
{
    auto *layout = new QVBoxLayout(this);
    m_mode->addItems({QStringLiteral("Replace Text"), QStringLiteral("Add Text"), QStringLiteral("Format"), QStringLiteral("Change Case")});
    layout->addWidget(m_mode);

    auto *replacePage = new QWidget(this);
    auto *replaceForm = new QFormLayout(replacePage);
    replaceForm->addRow(QStringLiteral("Find:"), m_find);
    replaceForm->addRow(QStringLiteral("Replace with:"), m_replace);
    auto *replaceOptions = new QHBoxLayout;
    replaceOptions->addWidget(m_matchCase);
    replaceOptions->addWidget(m_regex);
    replaceOptions->addStretch();
    replaceForm->addRow(QString(), replaceOptions);
    m_pages->addWidget(replacePage);

    auto *addPage = new QWidget(this);
    auto *addForm = new QFormLayout(addPage);
    addForm->addRow(QStringLiteral("Text:"), m_addText);
    m_addWhere->addItems({QStringLiteral("after name"), QStringLiteral("before name")});
    addForm->addRow(QStringLiteral("Where:"), m_addWhere);
    m_pages->addWidget(addPage);

    auto *formatPage = new QWidget(this);
    auto *formatForm = new QFormLayout(formatPage);
    m_format->addItems({QStringLiteral("Name and Index"), QStringLiteral("Name and Counter"), QStringLiteral("Name and Date")});
    formatForm->addRow(QStringLiteral("Name format:"), m_format);
    m_formatWhere->addItems({QStringLiteral("after name"), QStringLiteral("before name")});
    formatForm->addRow(QStringLiteral("Where:"), m_formatWhere);
    m_customName->setText(QStringLiteral("File"));
    formatForm->addRow(QStringLiteral("Custom format:"), m_customName);
    m_startAt->setRange(0, 999999);
    m_startAt->setValue(1);
    formatForm->addRow(QStringLiteral("Start numbers at:"), m_startAt);
    m_pages->addWidget(formatPage);

    auto *casePage = new QWidget(this);
    auto *caseForm = new QFormLayout(casePage);
    m_case->addItems({QStringLiteral("lowercase"), QStringLiteral("UPPERCASE"), QStringLiteral("Title Case")});
    caseForm->addRow(QStringLiteral("Change to:"), m_case);
    m_pages->addWidget(casePage);
    layout->addWidget(m_pages);

    m_preview->setColumnCount(2);
    m_preview->setHeaderLabels({QStringLiteral("Name"), QStringLiteral("New Name")});
    m_preview->setRootIsDecorated(false);
    m_preview->setUniformRowHeights(true);
    m_preview->setMinimumHeight(PREVIEW_MIN_HEIGHT);
    m_preview->header()->setSectionResizeMode(QHeaderView::Stretch);
    m_preview->setAccessibleName(QStringLiteral("New names"));
    layout->addWidget(m_preview, 1);
    layout->addWidget(m_summary);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_renameButton = buttons->addButton(QStringLiteral("Rename"), QDialogButtonBox::AcceptRole);
    m_renameButton->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    // A stacked widget is as tall as its tallest page; only the page on screen counts here, so the preview starts right
    // under the controls of the chosen mode.
    const auto showPage = [this](int index) {
        for (int i = 0; i < m_pages->count(); ++i) {
            const auto policy = i == index ? QSizePolicy::Preferred : QSizePolicy::Ignored;
            m_pages->widget(i)->setSizePolicy(policy, policy);
        }
        m_pages->setCurrentIndex(index);
        m_pages->adjustSize();
    };
    connect(m_mode, &QComboBox::currentIndexChanged, this, showPage);
    showPage(0);
    const auto changed = [this] {
        if (!m_settingRule) {
            refresh();
        }
    };
    for (QLineEdit *field : {m_find, m_replace, m_addText, m_customName}) {
        connect(field, &QLineEdit::textChanged, this, changed);
    }
    for (QComboBox *box : {m_mode, m_addWhere, m_format, m_formatWhere, m_case}) {
        connect(box, &QComboBox::currentIndexChanged, this, changed);
    }
    for (QCheckBox *box : {m_matchCase, m_regex}) {
        connect(box, &QCheckBox::toggled, this, changed);
    }
    connect(m_startAt, &QSpinBox::valueChanged, this, changed);
    m_find->setFocus();
}

void BatchRenameSheet::readFolders()
{
    m_namesInFolders.clear();
    for (const KFileItem &item : std::as_const(m_items)) {
        const QString key = folderKey(item.url());
        if (m_namesInFolders.contains(key)) {
            continue;
        }
        QSet<QString> names;
        // A folder elsewhere (a phone, a network share) is not listed here: KIO's rename refuses a taken name there,
        // and the batch stops with its message.
        if (item.url().isLocalFile()) {
            const QDir folder(QFileInfo(item.url().toLocalFile()).absolutePath());
            const QStringList all = folder.entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
            names = QSet<QString>(all.cbegin(), all.cend());
        }
        m_namesInFolders.insert(key, names);
    }
}

RenameRule BatchRenameSheet::rule() const
{
    RenameRule rule;
    rule.mode = static_cast<RenameMode>(m_mode->currentIndex());
    rule.find = m_find->text();
    rule.replaceWith = m_replace->text();
    rule.matchCase = m_matchCase->isChecked();
    rule.regularExpression = m_regex->isChecked();
    rule.text = m_addText->text();
    rule.position = m_addWhere->currentIndex() == 1 ? RenamePosition::Before : RenamePosition::After;
    rule.format = static_cast<RenameFormat>(m_format->currentIndex());
    rule.numberPosition = m_formatWhere->currentIndex() == 1 ? RenamePosition::Before : RenamePosition::After;
    rule.customName = m_customName->text();
    rule.startAt = m_startAt->value();
    rule.letterCase = static_cast<RenameCase>(m_case->currentIndex());
    return rule;
}

void BatchRenameSheet::setRule(const RenameRule &rule)
{
    m_settingRule = true;
    m_mode->setCurrentIndex(static_cast<int>(rule.mode));
    m_find->setText(rule.find);
    m_replace->setText(rule.replaceWith);
    m_matchCase->setChecked(rule.matchCase);
    m_regex->setChecked(rule.regularExpression);
    m_addText->setText(rule.text);
    m_addWhere->setCurrentIndex(rule.position == RenamePosition::Before ? 1 : 0);
    m_format->setCurrentIndex(static_cast<int>(rule.format));
    m_formatWhere->setCurrentIndex(rule.numberPosition == RenamePosition::Before ? 1 : 0);
    m_customName->setText(rule.customName);
    m_startAt->setValue(rule.startAt);
    m_case->setCurrentIndex(static_cast<int>(rule.letterCase));
    m_settingRule = false;
    refresh();
}

const RenamePlan &BatchRenameSheet::plan() const
{
    return m_plan;
}

void BatchRenameSheet::refresh()
{
    m_plan = planRenames(m_renameItems, rule(), m_namesInFolders);
    m_preview->setUpdatesEnabled(false);
    m_preview->clear();
    QList<QTreeWidgetItem *> rows;
    rows.reserve(m_plan.rows.size());
    qsizetype problems = 0;
    for (qsizetype i = 0; i < m_plan.rows.size(); ++i) {
        const RenameRow &row = m_plan.rows.at(i);
        auto *line = new QTreeWidgetItem({m_renameItems.at(i).name, row.newName});
        if (row.newName != m_renameItems.at(i).name && row.problem != RenameProblem::None) {
            ++problems;
            line->setForeground(1, PROBLEM_RED);
            line->setToolTip(1, renameProblemText(row.problem));
            line->setText(1, row.newName.isEmpty() ? renameProblemText(row.problem) : row.newName);
        } else if (row.newName == m_renameItems.at(i).name) {
            line->setForeground(1, palette().color(QPalette::PlaceholderText));
        }
        rows.append(line);
    }
    m_preview->addTopLevelItems(rows);
    m_preview->setUpdatesEnabled(true);
    if (!m_plan.ruleError.isEmpty()) {
        m_summary->setText(QStringLiteral("The regular expression is not valid: %1").arg(m_plan.ruleError));
    } else if (problems > 0) {
        m_summary->setText(problems == 1 ? QStringLiteral("1 name cannot be used; point at it to see why")
                                         : QStringLiteral("%1 names cannot be used; point at one to see why").arg(problems));
    } else {
        m_summary->setText(m_plan.changed == 1 ? QStringLiteral("1 item will be renamed")
                                               : QStringLiteral("%1 items will be renamed").arg(m_plan.changed));
    }
    m_renameButton->setEnabled(m_plan.canRun());
}

void BatchRenameSheet::accept()
{
    // The folders may have changed while the sheet was open: plan once more against what is there now.
    readFolders();
    refresh();
    if (!m_plan.canRun()) {
        return;
    }
    QList<QUrl> sources;
    QStringList newNames;
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        if (m_plan.rows.at(i).newName != m_items.at(i).name()) {
            sources.append(m_items.at(i).url());
            newNames.append(m_plan.rows.at(i).newName);
        }
    }
    // KIO calls the function once per source, in order (BatchRenameJobPrivate::slotStart, batchrenamejob.cpp, KIO
    // 6.31.0), and keeps a new name that ends with the old suffix as it is; splitExtension keeps every suffix.
    auto *job = KIO::batchRenameWithFunction(sources, [newNames, next = qsizetype(0)](QStringView) mutable {
        return next < newNames.size() ? newNames.at(next++) : QString();
    });
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, parentWidget()));
    KJobWidgets::setWindow(job, parentWidget());
    // One entry in the undo history for the whole batch, as Dolphin records its own batch rename.
    KIO::FileUndoManager::self()->recordJob(KIO::FileUndoManager::BatchRename, sources, QUrl(), job);
    auto done = std::make_shared<QList<QUrl>>();
    connect(job, &KIO::BatchRenameJob::fileRenamed, this, [done](const QUrl &, const QUrl &now) { done->append(now); });
    // The sheet hides at once and lives until the job ends, so the new names reach whoever listens; then it closes.
    connect(job, &KJob::result, this, [this, done] {
        Q_EMIT renamed(*done);
        close();
    });
    hide();
}

} // namespace fe
