// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements addressfield.h.

#include "addressfield.h"

#include "core/address.h"
#include "crumbstrip.h"
#include "style.h"

#include <QAbstractItemView>
#include <QCompleter>
#include <QDir>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QMimeData>
#include <QPainter>
#include <QPainterPath>
#include <QStandardItemModel>
#include <QTimer>
#include <QToolTip>

namespace fe {

namespace {

/// How many suggestions drop down at most.
constexpr int SUGGESTION_LIMIT = 12;
/// How long the red ring of a path that leads nowhere stays.
constexpr int ERROR_MS = 1200;
constexpr int ICON_SIZE = 16;
constexpr int ICON_LEFT = 13;
/// The role that holds a suggestion's whole path, which the field takes when it is chosen.
constexpr int PATH_ROLE = Qt::UserRole + 1;
/// The suggestion list: a dark card with roomy rows and the accent on the row under the keys.
constexpr auto POPUP_STYLE = "QListView { background: rgb(36, 36, 40); color: rgba(255, 255, 255, 236);"
                             " border: 1px solid rgba(255, 255, 255, 40); padding: 5px; outline: 0; }"
                             "QListView::item { height: 26px; padding: 0 6px; border-radius: 6px; }"
                             "QListView::item:selected, QListView::item:hover { background: rgb(22, 118, 212); color: white; }";

} // namespace

AddressField::AddressField(QString homePath, QWidget *parent)
    : QLineEdit(parent)
    , m_homePath(std::move(homePath))
    , m_suggestions(new QStandardItemModel(this))
    , m_completer(new QCompleter(m_suggestions, this))
    , m_errorTimer(new QTimer(this))
    , m_crumbs(new CrumbStrip(this, false, Qt::IBeamCursor))
{
    setFrame(false);
    // A click on a segment opens it, so only a click beside the segments takes the keyboard (mousePressEvent).
    setFocusPolicy(Qt::TabFocus);
    setMouseTracking(true);
    setFont(style::uiFont(13));
    setFixedHeight(style::CAPSULE_HEIGHT);
    setTextMargins(ICON_LEFT + ICON_SIZE + 6, 0, 12, 0);
    setAccessibleName(QStringLiteral("Address"));
    setToolTip(QStringLiteral("Type a path to go there (Ctrl+L)"));
    QPalette fieldPalette = palette();
    fieldPalette.setColor(QPalette::Base, Qt::transparent);
    fieldPalette.setColor(QPalette::Text, style::TEXT);
    fieldPalette.setColor(QPalette::PlaceholderText, style::TEXT_SECONDARY);
    setPalette(fieldPalette);
    setAttribute(Qt::WA_MacShowFocusRect, false);

    // The model already holds only what matches, in order, so the popup shows it as it is.
    m_completer->setCompletionMode(QCompleter::UnfilteredPopupCompletion);
    m_completer->setMaxVisibleItems(SUGGESTION_LIMIT);
    m_completer->setCompletionRole(PATH_ROLE);
    m_completer->setWidget(this);
    QAbstractItemView *popup = m_completer->popup();
    popup->setFont(style::uiFont(13));
    popup->setStyleSheet(QString::fromLatin1(POPUP_STYLE));
    popup->setIconSize(QSize(16, 16));
    connect(m_completer, qOverload<const QString &>(&QCompleter::activated), this, [this](const QString &chosen) {
        setText(chosen);
        Q_EMIT addressEntered(chosen);
    });
    connect(this, &QLineEdit::textEdited, this, &AddressField::refreshSuggestions);
    connect(this, &QLineEdit::returnPressed, this, [this] { Q_EMIT addressEntered(text()); });

    m_errorTimer->setSingleShot(true);
    m_errorTimer->setInterval(ERROR_MS);
    connect(m_errorTimer, &QTimer::timeout, this, qOverload<>(&QWidget::update));
}

void AddressField::setLocation(const QUrl &url)
{
    m_location = url;
    if (!hasFocus()) {
        showLocation();
    }
}

void AddressField::setCrumbs(const QList<Crumb> &crumbs)
{
    m_crumbs->setCrumbs(crumbs);
}

CrumbStrip *AddressField::crumbs() const
{
    return m_crumbs;
}

void AddressField::placeCrumbs()
{
    m_crumbs->setArea(QRectF(rect().adjusted(textMargins().left(), 0, -textMargins().right(), 0)));
}

void AddressField::showLocation()
{
    setText(addressText(m_location));
    // A long path shows its end, where the folder on screen is.
    setCursorPosition(int(text().size()));
}

void AddressField::startEditing()
{
    setFocus(Qt::ShortcutFocusReason);
    selectAll();
}

void AddressField::showError()
{
    m_errorTimer->start();
    update();
}

QStringList AddressField::subfolderNames(const QString &typedText) const
{
    const QString folderText = splitTyped(typedText).folderText;
    if (folderText.isEmpty()) {
        return {};
    }
    const QUrl folder = typedLocation(folderText, m_homePath, m_location);
    if (!folder.isLocalFile()) {
        return {};
    }
    return QDir(folder.toLocalFile()).entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);
}

void AddressField::refreshSuggestions()
{
    const QString typed = text();
    const QStringList shown = addressSuggestions(typed, subfolderNames(typed), SUGGESTION_LIMIT);
    m_suggestions->clear();
    const QIcon folder(style::tintedIcon(QStringLiteral(":/icons/actions/folder.svg"), 16, style::ACCENT, devicePixelRatioF()));
    for (const QString &path : shown) {
        // The name alone reads at a glance; the field takes the whole path.
        auto *item = new QStandardItem(folder, path.section(u'/', -2, -2));
        item->setData(path, PATH_ROLE);
        m_suggestions->appendRow(item);
    }
    if (shown.isEmpty() || (shown.size() == 1 && shown.constFirst() == typed)) {
        m_completer->popup()->hide();
        return;
    }
    // The list drops down under the path, as wide as the field.
    m_completer->complete(QRect(0, 4, width(), height()));
}

bool AddressField::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip && !hasFocus()) {
        const auto *help = static_cast<QHelpEvent *>(event);
        if (const QString tip = m_crumbs->toolTipAt(help->pos()); !tip.isEmpty()) {
            QToolTip::showText(help->globalPos(), tip, this);
            return true;
        }
    }
    // Tab completes instead of moving the focus on.
    if (event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Tab && key->modifiers() == Qt::NoModifier) {
            const QString completed = tabCompleted(text(), subfolderNames(text()));
            if (completed != text()) {
                setText(completed);
            }
            refreshSuggestions();
            return true;
        }
    }
    return QLineEdit::event(event);
}

void AddressField::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        m_completer->popup()->hide();
        showLocation();
        Q_EMIT editingCancelled();
        return;
    }
    QLineEdit::keyPressEvent(event);
}

void AddressField::focusInEvent(QFocusEvent *event)
{
    QLineEdit::focusInEvent(event);
    // Typing: no segment is under the pointer any more, and the cursor is the text cursor.
    m_crumbs->leave();
    // A click selects the whole path, ready to be typed over, as Explorer's and Finder's fields do.
    QTimer::singleShot(0, this, &QLineEdit::selectAll);
    update();
}

void AddressField::focusOutEvent(QFocusEvent *event)
{
    QLineEdit::focusOutEvent(event);
    // The popup takes the focus while it is open; the field keeps what is typed meanwhile.
    if (!m_completer->popup()->isVisible()) {
        showLocation();
    }
    update();
}

void AddressField::paintEvent(QPaintEvent *event)
{
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF bounds = QRectF(rect()).adjusted(1, 1, -1, -1);
        const qreal radius = bounds.height() / 2.0;
        style::paintGlass(painter, bounds, radius, hasFocus() ? style::CAPSULE_HOVER : style::CAPSULE_FILL);
        const bool error = m_errorTimer->isActive();
        if (hasFocus() || error) {
            style::paintFocusRing(painter, bounds, error ? style::CAPACITY_CRITICAL : style::ACCENT);
        }
        const QPixmap icon = style::tintedIcon(QStringLiteral(":/icons/actions/folder.svg"), ICON_SIZE,
                                               hasFocus() ? style::ACCENT : style::TEXT_SECONDARY, devicePixelRatioF());
        painter.drawPixmap(QPointF(ICON_LEFT, (height() - ICON_SIZE) / 2.0), icon);
        if (!hasFocus() && !m_crumbs->isEmpty()) {
            m_crumbs->paint(painter);
            return;
        }
        if (!hasFocus()) {
            // At rest a long path keeps its end, where the folder on screen is, and loses its start to an ellipsis.
            const QRect textRect = rect().adjusted(textMargins().left(), 0, -textMargins().right(), 0);
            painter.setFont(font());
            painter.setPen(style::TEXT);
            painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(text(), Qt::ElideLeft, textRect.width()));
            return;
        }
    }
    QLineEdit::paintEvent(event);
}

void AddressField::resizeEvent(QResizeEvent *event)
{
    QLineEdit::resizeEvent(event);
    placeCrumbs();
}

void AddressField::mousePressEvent(QMouseEvent *event)
{
    if (!hasFocus()) {
        if (m_crumbs->mousePress(event)) {
            return;
        }
        // Beside the segments a left click opens the field for typing; other buttons do nothing, so a middle click
        // cannot paste the primary selection into a field that is not being edited.
        if (event->button() != Qt::LeftButton) {
            event->accept();
            return;
        }
        setFocus(Qt::MouseFocusReason);
    }
    QLineEdit::mousePressEvent(event);
}

void AddressField::mouseMoveEvent(QMouseEvent *event)
{
    if (!hasFocus() && m_crumbs->mouseMove(event)) {
        return;
    }
    QLineEdit::mouseMoveEvent(event);
}

void AddressField::mouseReleaseEvent(QMouseEvent *event)
{
    if (!hasFocus() && m_crumbs->mouseRelease(event)) {
        return;
    }
    QLineEdit::mouseReleaseEvent(event);
}

void AddressField::leaveEvent(QEvent *event)
{
    if (!hasFocus()) {
        m_crumbs->leave();
    }
    QLineEdit::leaveEvent(event);
}

void AddressField::contextMenuEvent(QContextMenuEvent *event)
{
    if (!hasFocus() && m_crumbs->contextMenu(event)) {
        return;
    }
    QLineEdit::contextMenuEvent(event);
}

void AddressField::dragEnterEvent(QDragEnterEvent *event)
{
    if (!hasFocus()) {
        // At rest only files dropped on a segment count; text is typed, not dropped, into a field at rest.
        if (!m_crumbs->dragEnter(event)) {
            event->ignore();
        }
        return;
    }
    QLineEdit::dragEnterEvent(event);
}

void AddressField::dragMoveEvent(QDragMoveEvent *event)
{
    if (!hasFocus()) {
        if (!m_crumbs->dragMove(event)) {
            event->ignore();
        }
        return;
    }
    QLineEdit::dragMoveEvent(event);
}

void AddressField::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_crumbs->dragLeave();
    QLineEdit::dragLeaveEvent(event);
}

void AddressField::dropEvent(QDropEvent *event)
{
    if (!hasFocus()) {
        if (!m_crumbs->drop(event)) {
            event->ignore();
        }
        return;
    }
    QLineEdit::dropEvent(event);
}

} // namespace fe
