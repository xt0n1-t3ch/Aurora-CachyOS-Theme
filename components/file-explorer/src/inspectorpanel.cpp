// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements inspectorpanel.h.

#include "inspectorpanel.h"

#include "glassfield.h"
#include "style.h"
#include "toolbar.h"

#include <KDirWatch>
#include <KFormat>
#include <KIO/PreviewJob>

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QLabel>
#include <QPainter>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>

#include <sys/stat.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace fe {

namespace {

constexpr int WIDTH = 290;
constexpr int PREVIEW_SIZE = 160;
constexpr int FIELD_HEIGHT = 26;
/// Changes in a watched folder gather this long before the count runs again, in milliseconds.
constexpr int RECOUNT_MS = 600;

/// ioprio_set(2): the idle class, which reads only when no one else is reading.
void readOnlyWhenIdle()
{
    constexpr int ioprioWhoProcess = 1;
    constexpr int ioprioClassIdle = 3;
    constexpr int ioprioClassShift = 13;
    ::syscall(SYS_ioprio_set, ioprioWhoProcess, 0, ioprioClassIdle << ioprioClassShift);
}

QString sizeOf(qint64 bytes)
{
    return KFormat().formatByteSize(double(bytes));
}

QLabel *label(QWidget *parent, qreal size, const QColor &color, QFont::Weight weight = QFont::Normal)
{
    auto *text = new QLabel(parent);
    text->setFont(style::uiFont(size, weight));
    QPalette colors = text->palette();
    colors.setColor(QPalette::WindowText, color);
    text->setPalette(colors);
    text->setWordWrap(true);
    text->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return text;
}

QString dateOf(const QDateTime &time)
{
    return time.isValid() ? KFormat().formatRelativeDateTime(time, QLocale::ShortFormat) : QStringLiteral("Unknown");
}

/// Runs work on a new thread at the given priority and deletes the thread when it ends.
QThread *runAside(std::function<void()> work, QThread::Priority priority)
{
    QThread *thread = QThread::create(std::move(work));
    QObject::connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start(priority);
    return thread;
}

/// Wires a watch to the timer that counts again.
void watchInto(KDirWatch *watch, QTimer *recount)
{
    QObject::connect(watch, &KDirWatch::dirty, recount, qOverload<>(&QTimer::start));
    QObject::connect(watch, &KDirWatch::created, recount, qOverload<>(&QTimer::start));
    QObject::connect(watch, &KDirWatch::deleted, recount, qOverload<>(&QTimer::start));
}

} // namespace

InspectorPanel::InspectorPanel(QWidget *parent)
    : QWidget(parent)
    , m_watch(new KDirWatch(this))
    , m_recount(new QTimer(this))
    , m_preview(new QLabel(this))
    , m_name(label(this, 15, style::TEXT, QFont::DemiBold))
    , m_kind(label(this, 12, style::TEXT_SECONDARY))
    , m_rows(new QFormLayout)
    , m_size(nullptr)
    , m_hashBox(new QWidget(this))
    , m_hashKind(new QComboBox(m_hashBox))
    , m_hashButton(new GlassButton(QString(), QStringLiteral("Compute the file's hash"), m_hashBox))
    , m_hash(label(m_hashBox, 11, style::TEXT))
    , m_copyHash(new GlassButton(QStringLiteral("edit-copy-symbolic"), QStringLiteral("Copy the hash"), m_hashBox))
    , m_compare(new GlassLineEdit(QStringLiteral("edit-paste-symbolic"), FIELD_HEIGHT, m_hashBox))
    , m_verdict(label(m_hashBox, 12, style::TEXT_SECONDARY))
{
    setFixedWidth(WIDTH);
    setAccessibleName(QStringLiteral("Inspector"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(6);
    m_preview->setFixedHeight(PREVIEW_SIZE);
    m_preview->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_preview);
    layout->addSpacing(6);
    m_name->setAlignment(Qt::AlignHCenter);
    m_kind->setAlignment(Qt::AlignHCenter);
    layout->addWidget(m_name);
    layout->addWidget(m_kind);
    layout->addSpacing(10);
    m_rows->setLabelAlignment(Qt::AlignRight | Qt::AlignTop);
    m_rows->setHorizontalSpacing(10);
    m_rows->setVerticalSpacing(6);
    layout->addLayout(m_rows);

    auto *hashLayout = new QVBoxLayout(m_hashBox);
    hashLayout->setContentsMargins(0, 10, 0, 0);
    hashLayout->setSpacing(6);
    auto *hashRow = new QHBoxLayout;
    for (const HashKind &kind : hashKinds()) {
        m_hashKind->addItem(kind.name, int(kind.algorithm));
    }
    m_hashKind->setAccessibleName(QStringLiteral("Hash"));
    m_hashButton->setLabel(QStringLiteral("Compute"));
    m_hashButton->setStandalone(true);
    m_copyHash->setStandalone(true);
    m_copyHash->hide();
    hashRow->addWidget(m_hashKind, 1);
    hashRow->addWidget(m_hashButton);
    hashLayout->addLayout(hashRow);
    auto *resultRow = new QHBoxLayout;
    m_hash->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    m_hash->setWordWrap(true);
    // The hex is shown in groups with zero-width spaces between them, so it wraps; it is copied with the button, not
    // by selecting, which would take those spaces along.
    m_hash->setTextInteractionFlags(Qt::NoTextInteraction);
    m_hash->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    resultRow->addWidget(m_hash, 1);
    resultRow->addWidget(m_copyHash, 0, Qt::AlignTop);
    hashLayout->addLayout(resultRow);
    m_compare->setPlaceholderText(QStringLiteral("Paste a hash to compare"));
    m_compare->setAccessibleName(QStringLiteral("Hash to compare"));
    m_compare->setClearButtonEnabled(true);
    hashLayout->addWidget(m_compare);
    hashLayout->addWidget(m_verdict);
    layout->addWidget(m_hashBox);
    layout->addStretch();

    m_recount->setSingleShot(true);
    m_recount->setInterval(RECOUNT_MS);
    connect(m_recount, &QTimer::timeout, this, [this] {
        // A changed item is read again (its size, its dates) before the rows are filled anew.
        for (KFileItem &item : m_items) {
            item.refresh();
        }
        refresh();
    });
    watchInto(m_watch, m_recount);
    connect(m_hashButton, &QAbstractButton::clicked, this, &InspectorPanel::startHash);
    connect(m_hashKind, &QComboBox::currentIndexChanged, this, [this] {
        // A hash of another kind is computed anew.
        if (m_hashCancel) {
            *m_hashCancel = true;
        }
        m_hash->clear();
        m_hashHex.clear();
        m_copyHash->hide();
        updateCompare();
    });
    connect(m_copyHash, &QAbstractButton::clicked, this, [this] { QApplication::clipboard()->setText(m_hashHex); });
    connect(m_compare, &QLineEdit::textChanged, this, &InspectorPanel::updateCompare);
}

InspectorPanel::~InspectorPanel()
{
    for (const auto &cancel : {m_countCancel, m_hashCancel}) {
        if (cancel) {
            *cancel = true;
        }
    }
    // A thread may still be in a read; it checks its cancel flag between entries and blocks, so this wait is short.
    for (const QPointer<QThread> &thread : std::as_const(m_threads)) {
        if (thread) {
            thread->wait();
        }
    }
}

void InspectorPanel::setItems(const KFileItemList &items, const QUrl &folder)
{
    const QList<QUrl> urls = items.isEmpty() ? QList<QUrl>{folder} : items.urlList();
    if (urls == m_shownUrls && isVisible()) {
        return;
    }
    m_shownUrls = urls;
    m_items = items;
    m_folder = folder;
    if (m_items.isEmpty() && folder.isValid()) {
        m_items.append(KFileItem(folder));
        m_items.first().refresh();
    }
    m_compare->clear();
    if (isVisible()) {
        refresh();
    }
}

void InspectorPanel::stopWork()
{
    if (m_countCancel) {
        *m_countCancel = true;
    }
    if (m_hashCancel) {
        *m_hashCancel = true;
    }
    m_countCancel.reset();
    m_hashCancel.reset();
    m_recount->stop();
    // A fresh watch drops every path of the old one at once, whatever was added to it.
    delete m_watch;
    m_watch = new KDirWatch(this);
    watchInto(m_watch, m_recount);
    m_threads.removeAll(nullptr);
}

QStringList InspectorPanel::localPaths() const
{
    QStringList paths;
    for (const KFileItem &item : m_items) {
        const QString path = item.localPath();
        if (!path.isEmpty()) {
            paths.append(path);
        }
    }
    return paths;
}

void InspectorPanel::refresh()
{
    stopWork();
    while (m_rows->rowCount() > 0) {
        m_rows->removeRow(0);
    }
    m_size = label(this, 12, style::TEXT);
    const auto addRow = [this](const QString &title, QWidget *value) {
        QLabel *name = label(this, 12, style::TEXT_SECONDARY);
        name->setText(title);
        name->setWordWrap(false);
        m_rows->addRow(name, value);
    };
    const auto addText = [&](const QString &title, const QString &text) {
        QLabel *value = label(this, 12, style::TEXT);
        value->setText(text);
        addRow(title, value);
    };
    const bool single = m_items.size() == 1;
    const KFileItem first = m_items.value(0);
    if (single) {
        m_name->setText(first.text());
        m_kind->setText(first.mimeComment());
    } else {
        m_name->setText(QStringLiteral("%1 items").arg(m_items.size()));
        m_kind->setText(QString());
    }
    addRow(QStringLiteral("Size"), m_size);
    if (single) {
        const QUrl parent = first.url().adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
        QString where = parent.isLocalFile() ? parent.toLocalFile() : parent.toDisplayString();
        if (const QString home = QDir::homePath(); where == home || where.startsWith(home + QLatin1Char('/'))) {
            where = QLatin1Char('~') + where.mid(home.size());
        }
        QLabel *whereLabel = label(this, 12, style::TEXT);
        // A path has no spaces to wrap at: a zero-width space after each slash lets it break there. The text is not
        // selectable, so no one copies those spaces; the whole path is in the tooltip.
        whereLabel->setText(QString(where).replace(QLatin1Char('/'), QStringLiteral("/\u200B")));
        whereLabel->setTextInteractionFlags(Qt::NoTextInteraction);
        whereLabel->setToolTip(where);
        addRow(QStringLiteral("Where"), whereLabel);
        addText(QStringLiteral("Created"), dateOf(first.time(KFileItem::CreationTime)));
        addText(QStringLiteral("Modified"), dateOf(first.time(KFileItem::ModificationTime)));
        addText(QStringLiteral("Opened"), dateOf(first.time(KFileItem::AccessTime)));
        if (first.isLink()) {
            addText(QStringLiteral("Links to"), first.linkDest());
        }
        addText(QStringLiteral("Owner"), QStringLiteral("%1, group %2").arg(first.user(), first.group()));
        addText(QStringLiteral("Permissions"), permissionsText(first.permissions() | (first.isDir() ? S_IFDIR : 0)));
    }
    showPreview();
    const bool hashable = single && first.isLocalFile() && first.isFile() && !first.isDir();
    m_hashBox->setVisible(hashable);
    m_hash->clear();
    m_hashHex.clear();
    m_copyHash->hide();
    updateCompare();
    startCount();
}

void InspectorPanel::showPreview()
{
    const KFileItem first = m_items.value(0);
    const QIcon icon = QIcon::fromTheme(m_items.size() == 1 ? first.iconName() : QStringLiteral("document-multiple"));
    m_preview->setPixmap(icon.pixmap(QSize(PREVIEW_SIZE * 3 / 4, PREVIEW_SIZE * 3 / 4), devicePixelRatioF()));
    if (m_items.size() != 1 || first.isDir()) {
        return;
    }
    // The file's own picture where a thumbnailer knows its kind (images, videos, documents), as Dolphin's previews.
    auto *job = KIO::filePreview(m_items, QSize(PREVIEW_SIZE * 2, PREVIEW_SIZE));
    job->setScaleType(KIO::PreviewJob::Scaled);
    const QUrl wanted = first.url();
    connect(job, &KIO::PreviewJob::gotPreview, this, [this, wanted](const KFileItem &item, const QPixmap &pixmap) {
        if (item.url() == wanted && m_items.size() == 1 && m_items.first().url() == wanted) {
            m_preview->setPixmap(pixmap.scaled(QSize(WIDTH - 32, PREVIEW_SIZE) * pixmap.devicePixelRatio(), Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation));
        }
    });
}

void InspectorPanel::startCount()
{
    const QStringList paths = localPaths();
    bool folders = false;
    qint64 bytes = 0;
    for (const KFileItem &item : std::as_const(m_items)) {
        folders = folders || item.isDir();
        bytes += item.isDir() ? 0 : qint64(item.size());
    }
    if (!folders || paths.size() != m_items.size()) {
        // Files only, or items elsewhere than on this PC: their listed sizes are all there is to show.
        m_size->setText(sizeOf(bytes));
        for (const QString &path : paths) {
            m_watch->addFile(path);
        }
        Q_EMIT sizeCounted();
        return;
    }
    for (const QString &path : paths) {
        if (QFileInfo(path).isDir()) {
            // The folder's own listing: an item added, removed or renamed in it counts it again. Deeper changes show
            // when the Inspector shows the folder again.
            m_watch->addDir(path);
        } else {
            m_watch->addFile(path);
        }
    }
    m_size->setText(QStringLiteral("Counting"));
    auto cancel = std::make_shared<std::atomic_bool>(false);
    m_countCancel = cancel;
    QPointer<InspectorPanel> panel(this);
    QPointer<QLabel> sizeLabel(m_size);
    // The panel lives on the main thread; results reach it as queued calls that check it and the run are still current.
    const auto post = [panel, cancel, sizeLabel](Tally tally, bool done) {
        QMetaObject::invokeMethod(qApp, [panel, cancel, sizeLabel, tally, done] {
            if (!panel || !sizeLabel || *cancel || panel->m_countCancel != cancel) {
                return;
            }
            sizeLabel->setText(tallyText(tally, done, sizeOf));
            if (done) {
                Q_EMIT panel->sizeCounted();
            }
        }, Qt::QueuedConnection);
    };
    m_threads.append(runAside([paths, cancel, post] {
        readOnlyWhenIdle();
        Tally tally;
        const bool finished = tallyTree(paths, *cancel, tally, [&post](const Tally &sofar) { post(sofar, false); }, 200);
        if (finished) {
            post(tally, true);
        }
    }, QThread::IdlePriority));
}

void InspectorPanel::startHash()
{
    const QStringList paths = localPaths();
    if (paths.size() != 1) {
        return;
    }
    if (m_hashCancel) {
        *m_hashCancel = true;
    }
    auto cancel = std::make_shared<std::atomic_bool>(false);
    m_hashCancel = cancel;
    const auto algorithm = QCryptographicHash::Algorithm(m_hashKind->currentData().toInt());
    const qint64 total = qint64(m_items.first().size());
    m_hash->setText(QStringLiteral("Reading"));
    m_hashHex.clear();
    m_copyHash->hide();
    QPointer<InspectorPanel> panel(this);
    const auto post = [panel, cancel](const QString &text, bool done) {
        QMetaObject::invokeMethod(qApp, [panel, cancel, text, done] {
            if (!panel || *cancel || panel->m_hashCancel != cancel) {
                return;
            }
            if (done) {
                panel->m_hashHex = text;
                QString groups;
                for (qsizetype at = 0; at < text.size(); at += 8) {
                    groups += text.mid(at, 8) + QChar(0x200B);
                }
                panel->m_hash->setText(text.isEmpty() ? QStringLiteral("The file could not be read") : groups);
            } else {
                panel->m_hash->setText(text);
            }
            panel->m_copyHash->setVisible(done && !text.isEmpty());
            if (done) {
                panel->updateCompare();
                Q_EMIT panel->hashComputed();
            }
        }, Qt::QueuedConnection);
    };
    const QString path = paths.first();
    m_threads.append(runAside([path, algorithm, cancel, post, total] {
        int lastPercent = -1;
        const std::optional<QString> hex = hashFile(path, algorithm, *cancel, [&](qint64 done) {
            const int percent = total > 0 ? int(done * 100 / total) : 0;
            if (percent != lastPercent) {
                lastPercent = percent;
                post(QStringLiteral("Reading, %1%").arg(percent), false);
            }
        });
        if (!*cancel) {
            post(hex.value_or(QString()), true);
        }
    }, QThread::LowPriority));
}

void InspectorPanel::updateCompare()
{
    const bool computed = m_copyHash->isVisible();
    switch (computed ? checkHash(m_hashHex, m_compare->text()) : HashCheck::Empty) {
    case HashCheck::Empty:
        m_verdict->setText(m_compare->text().trimmed().isEmpty() || computed ? QString() : QStringLiteral("Compute the hash to compare"));
        m_verdict->setPalette(m_kind->palette());
        return;
    case HashCheck::Match: {
        m_verdict->setText(QStringLiteral("Matches: the file is the one the hash describes"));
        QPalette green = m_verdict->palette();
        green.setColor(QPalette::WindowText, QColor(48, 209, 88));
        m_verdict->setPalette(green);
        return;
    }
    case HashCheck::Mismatch: {
        m_verdict->setText(QStringLiteral("Does not match: the file differs"));
        QPalette red = m_verdict->palette();
        red.setColor(QPalette::WindowText, style::CAPACITY_CRITICAL);
        m_verdict->setPalette(red);
        return;
    }
    case HashCheck::NotThisKind:
        m_verdict->setText(QStringLiteral("Not a %1 hash").arg(m_hashKind->currentText()));
        m_verdict->setPalette(m_kind->palette());
        return;
    }
}

QString InspectorPanel::sizeText() const
{
    return m_size ? m_size->text() : QString();
}

QString InspectorPanel::hashText() const
{
    return m_hashHex;
}

QString InspectorPanel::compareText() const
{
    return m_verdict->text();
}

void InspectorPanel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    style::paintGlass(painter, QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), style::SIDEBAR_RADIUS,
                      window()->isActiveWindow() ? style::SIDEBAR_GLASS : style::SIDEBAR_GLASS_INACTIVE);
}

void InspectorPanel::hideEvent(QHideEvent *event)
{
    stopWork();
    QWidget::hideEvent(event);
}

void InspectorPanel::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    if (!m_items.isEmpty()) {
        refresh();
    }
}

} // namespace fe
