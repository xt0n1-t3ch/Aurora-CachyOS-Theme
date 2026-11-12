// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Implements crumbaccessible.h: the path bar as a group whose children are its segments, each a link with a press
// action. The segments are painted, not widgets, so each child is an interface registered with QAccessible and
// replaced whenever the strip lays out anew.

#include "crumbaccessible.h"

#include "bottombar.h"
#include "crumbstrip.h"

#include <QAccessibleWidget>
#include <QPointer>
#include <QWidget>
#include <QWindow>

namespace fe {

namespace {

/// One path segment (or the ellipsis) of a strip, by its position in the strip.
class SegmentAccessible : public QAccessibleInterface, public QAccessibleActionInterface
{
public:
    SegmentAccessible(QWidget *host, CrumbStrip *strip, qsizetype slot)
        : m_host(host)
        , m_strip(strip)
        , m_slot(slot)
    {
    }

    bool isValid() const override { return m_host && m_strip && m_slot < m_strip->slotCount(); }
    QObject *object() const override { return nullptr; }
    QWindow *window() const override { return m_host ? m_host->window()->windowHandle() : nullptr; }
    QList<std::pair<QAccessibleInterface *, QAccessible::Relation>> relations(QAccessible::Relation) const override { return {}; }
    QAccessibleInterface *focusChild() const override { return nullptr; }
    QAccessibleInterface *childAt(int, int) const override { return nullptr; }
    QAccessibleInterface *parent() const override { return QAccessible::queryAccessibleInterface(m_host); }
    QAccessibleInterface *child(int) const override { return nullptr; }
    int childCount() const override { return 0; }
    int indexOfChild(const QAccessibleInterface *) const override { return -1; }
    void setText(QAccessible::Text, const QString &) override {}
    QAccessible::Role role() const override { return QAccessible::Link; }

    QString text(QAccessible::Text kind) const override
    {
        if (!isValid()) {
            return {};
        }
        if (kind == QAccessible::Name) {
            return m_strip->slotName(m_slot);
        }
        if (kind == QAccessible::Description) {
            return m_strip->toolTipAt(m_strip->slotRect(m_slot).center());
        }
        return {};
    }

    QRect rect() const override
    {
        if (!isValid()) {
            return {};
        }
        const QRect local = m_strip->slotRect(m_slot).toAlignedRect();
        return {m_host->mapToGlobal(local.topLeft()), local.size()};
    }

    QAccessible::State state() const override
    {
        QAccessible::State state;
        state.invisible = !isValid() || !m_host->isVisible();
        state.linked = true;
        return state;
    }

    void *interface_cast(QAccessible::InterfaceType type) override
    {
        return type == QAccessible::ActionInterface ? static_cast<QAccessibleActionInterface *>(this) : nullptr;
    }

    QStringList actionNames() const override { return {pressAction()}; }
    QStringList keyBindingsForAction(const QString &) const override { return {}; }

    void doAction(const QString &name) override
    {
        if (name == pressAction() && isValid()) {
            m_strip->activateSlot(m_slot);
        }
    }

private:
    QPointer<QWidget> m_host;
    QPointer<CrumbStrip> m_strip;
    qsizetype m_slot;
};

/// The path bar: a group of segment links.
class PathBarAccessible : public QAccessibleWidget
{
public:
    explicit PathBarAccessible(PathBar *bar)
        : QAccessibleWidget(bar, QAccessible::Grouping, QStringLiteral("Path"))
        , m_strip(bar->crumbs())
    {
    }

    ~PathBarAccessible() override { dropChildren(); }

    int childCount() const override { return m_strip ? int(m_strip->slotCount()) : 0; }

    QAccessibleInterface *child(int index) const override
    {
        syncChildren();
        return index >= 0 && index < m_children.size() ? QAccessible::accessibleInterface(m_children.at(index)) : nullptr;
    }

    int indexOfChild(const QAccessibleInterface *child) const override
    {
        syncChildren();
        for (qsizetype i = 0; i < m_children.size(); ++i) {
            if (QAccessible::accessibleInterface(m_children.at(i)) == child) {
                return int(i);
            }
        }
        return -1;
    }

    QAccessibleInterface *childAt(int x, int y) const override
    {
        syncChildren();
        for (const QAccessible::Id id : m_children) {
            QAccessibleInterface *child = QAccessible::accessibleInterface(id);
            if (child && child->rect().contains(x, y)) {
                return child;
            }
        }
        return nullptr;
    }

private:
    /// Makes one registered child per slot, again whenever the strip's slots changed since the last time.
    void syncChildren() const
    {
        if (!m_strip || (m_synced == m_strip->generation() && m_children.size() == m_strip->slotCount())) {
            return;
        }
        dropChildren();
        for (qsizetype slot = 0; slot < m_strip->slotCount(); ++slot) {
            m_children.append(QAccessible::registerAccessibleInterface(new SegmentAccessible(widget(), m_strip, slot)));
        }
        m_synced = m_strip->generation();
    }

    void dropChildren() const
    {
        for (const QAccessible::Id id : std::as_const(m_children)) {
            QAccessible::deleteAccessibleInterface(id);
        }
        m_children.clear();
    }

    QPointer<CrumbStrip> m_strip;
    mutable QList<QAccessible::Id> m_children;
    mutable quint64 m_synced = 0;
};

QAccessibleInterface *pathBarFactory(const QString &className, QObject *object)
{
    if (className == QLatin1String("fe::PathBar") && object && object->isWidgetType()) {
        return new PathBarAccessible(static_cast<PathBar *>(object));
    }
    return nullptr;
}

} // namespace

void installCrumbAccessibility()
{
    QAccessible::installFactory(pathBarFactory);
}

} // namespace fe
