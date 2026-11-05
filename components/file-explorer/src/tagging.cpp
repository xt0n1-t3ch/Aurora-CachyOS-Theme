// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements tagging.h.

#include "tagging.h"

#include "core/tags.h"
#include "style.h"

#include <KDirNotify>
#include <KFileMetaData/UserMetaData>

#include <QDir>
#include <QFileInfo>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStandardPaths>
#include <QWidgetAction>

namespace fe::tagging {

namespace {

constexpr int DOT_SIZE = 14;
constexpr int DOT_GAP = 10;
constexpr int DOTS_MARGIN = 14;
constexpr int DOTS_HEIGHT = 30;

/// The local paths of items, the only files tags can go on.
QStringList localPaths(const QList<QUrl> &items)
{
    QStringList paths;
    for (const QUrl &item : items) {
        if (item.isLocalFile()) {
            paths.append(item.toLocalFile());
        }
    }
    return paths;
}

QDir shelf(const QString &tag)
{
    QDir dir(shelfRoot() + u'/' + tag);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
    return dir;
}

/// Links path into the tag's shelf, unless a link to it is already there.
void shelve(const QString &tag, const QString &path)
{
    QDir dir = shelf(tag);
    const QStringList names = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QString &name : names) {
        if (QFileInfo(dir.filePath(name)).symLinkTarget() == path) {
            return;
        }
    }
    QFile::link(path, dir.filePath(shelfEntryName(QFileInfo(path).fileName(), names)));
}

/// Takes every link to path out of the tag's shelf.
void unshelve(const QString &tag, const QString &path)
{
    QDir dir = shelf(tag);
    for (const QFileInfo &entry : dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) {
        if (entry.isSymLink() && entry.symLinkTarget() == path) {
            QFile::remove(entry.filePath());
        }
    }
}

/// The color dots of a tag menu: one per color, filled when every item carries it; a click flips it for all.
class TagDots : public QWidget
{
public:
    TagDots(QList<QUrl> items, QMenu *menu)
        : QWidget(menu)
        , m_items(std::move(items))
        , m_menu(menu)
    {
        const auto count = int(colorTags().size());
        setFixedSize(2 * DOTS_MARGIN + count * DOT_SIZE + (count - 1) * DOT_GAP, DOTS_HEIGHT);
        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("Click a color to tag or untag"));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        for (int i = 0; i < colorTags().size(); ++i) {
            const ColorTag &tag = colorTags().at(i);
            const QRectF dot = dotRect(i);
            if (allTagged(m_items, tag.name)) {
                painter.setPen(Qt::NoPen);
                painter.setBrush(tag.color);
                painter.drawEllipse(dot);
            } else {
                painter.setBrush(i == m_hover ? QColor(tag.color.red(), tag.color.green(), tag.color.blue(), 70) : Qt::transparent);
                painter.setPen(QPen(tag.color, i == m_hover ? 2.2 : 1.6));
                painter.drawEllipse(dot.adjusted(1, 1, -1, -1));
            }
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        const int hover = dotAt(event->position());
        if (hover != m_hover) {
            m_hover = hover;
            update();
        }
    }

    void leaveEvent(QEvent *) override
    {
        m_hover = -1;
        update();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        const int index = dotAt(event->position());
        if (index < 0) {
            return;
        }
        const QString name = colorTags().at(index).name;
        setTag(m_items, name, !allTagged(m_items, name));
        m_menu->close();
    }

private:
    [[nodiscard]] QRectF dotRect(int index) const
    {
        return {qreal(DOTS_MARGIN + index * (DOT_SIZE + DOT_GAP)), (height() - DOT_SIZE) / 2.0, qreal(DOT_SIZE), qreal(DOT_SIZE)};
    }

    [[nodiscard]] int dotAt(QPointF pos) const
    {
        for (int i = 0; i < colorTags().size(); ++i) {
            if (dotRect(i).adjusted(-DOT_GAP / 2.0, -6, DOT_GAP / 2.0, 6).contains(pos)) {
                return i;
            }
        }
        return -1;
    }

    QList<QUrl> m_items;
    QMenu *m_menu;
    int m_hover = -1;
};

/// A round icon in the tag's color, for the Tags submenu.
QIcon dotIcon(const QColor &color)
{
    QPixmap pixmap(32, 32);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(QRectF(6, 6, 20, 20));
    return QIcon(pixmap);
}

} // namespace

QStringList tagsOf(const QUrl &url)
{
    if (!url.isLocalFile()) {
        return {};
    }
    return KFileMetaData::UserMetaData(url.toLocalFile()).tags();
}

bool allTagged(const QList<QUrl> &items, const QString &tag)
{
    const QStringList paths = localPaths(items);
    if (paths.isEmpty()) {
        return false;
    }
    for (const QString &path : paths) {
        if (!KFileMetaData::UserMetaData(path).tags().contains(tag, Qt::CaseInsensitive)) {
            return false;
        }
    }
    return true;
}

void setTag(const QList<QUrl> &items, const QString &tag, bool on)
{
    QList<QUrl> changed;
    for (const QString &path : localPaths(items)) {
        KFileMetaData::UserMetaData metadata(path);
        const QStringList before = metadata.tags();
        const QStringList after = taggedWith(before, tag, on);
        if (after != before && metadata.setTags(after) == KFileMetaData::UserMetaData::NoError) {
            changed.append(QUrl::fromLocalFile(path));
        }
        on ? shelve(tag, path) : unshelve(tag, path);
    }
    if (!changed.isEmpty()) {
        // The views refresh these items, and the tag emblem plugin draws the new dots.
        org::kde::KDirNotify::emitFilesChanged(changed);
    }
}

QString shelfRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/file-explorer/tags");
}

QUrl shelfUrl(const QString &tag)
{
    QDir dir = shelf(tag);
    for (const QFileInfo &entry : dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) {
        const QString target = entry.symLinkTarget();
        if (!entry.isSymLink() || !QFileInfo::exists(target)
            || !KFileMetaData::UserMetaData(target).tags().contains(tag, Qt::CaseInsensitive)) {
            QFile::remove(entry.filePath());
        }
    }
    return QUrl::fromLocalFile(dir.absolutePath());
}

void addTagEntries(QMenu &menu, const QList<QUrl> &items)
{
    if (localPaths(items).isEmpty()) {
        return;
    }
    auto *dots = new QWidgetAction(&menu);
    dots->setDefaultWidget(new TagDots(items, &menu));
    menu.addAction(dots);

    QMenu *submenu = menu.addMenu(QIcon(style::tintedIcon(QStringLiteral(":/icons/actions/tag.svg"), 16, style::TEXT, 2.0)),
                                  QStringLiteral("Tags"));
    for (const ColorTag &tag : colorTags()) {
        QAction *entry = submenu->addAction(dotIcon(tag.color), tag.name);
        entry->setCheckable(true);
        entry->setChecked(allTagged(items, tag.name));
        QObject::connect(entry, &QAction::toggled, &menu, [items, name = tag.name](bool on) { setTag(items, name, on); });
    }
}

} // namespace fe::tagging
