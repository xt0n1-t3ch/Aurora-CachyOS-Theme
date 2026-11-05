// SPDX-License-Identifier: GPL-2.0-or-later
// The KWin plugin that shows the pointing hand over window decoration buttons, as everything clickable in this theme
// does. KWin shows the arrow there and KDecoration3 gives a decoration no say, so the plugin swaps the mouse
// cursor's source while the pointer rests on a button; the class comment below has the details.
#include <cursor.h>
#include <cursorsource.h>
#include <input.h>
#include <input_event.h>
#include <input_event_spy.h>
#include <plugin.h>
#include <pointer_input.h>
#include <window.h>

#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationButton>

#include <QPointer>

namespace
{

// KWin picks the cursor of a decoration from the window's move/resize gravity: the arrow everywhere but the resize
// edges (Window::updateInteractiveMoveResizeCursor, window.cpp:2407 in KWin 6.8.2). This spy watches the pointer
// (an InputEventSpy sees every motion before KWin's filters) and, while it rests on a visible, enabled decoration
// button, shows the theme's "pointer" shape through the mouse Cursor's source; when it leaves, KWin's own source
// goes back. KWin reevaluates its sources only when the hovered surface or decoration changes, so the hand stays
// while the pointer moves over the button.
class PointingHand : public KWin::Plugin, public KWin::InputEventSpy
{
public:
    PointingHand()
        : m_hand(std::make_unique<KWin::ShapeCursorSource>())
    {
        KWin::input()->installInputEventSpy(this);
    }

    ~PointingHand() override
    {
        KWin::input()->uninstallInputEventSpy(this);
        restore();
    }

    // Shows the hand while the pointer is over a decoration button, and gives the cursor back otherwise.
    void pointerMotion(KWin::PointerMotionEvent *event) override
    {
        if (overButton(event->position)) {
            showHand();
        } else {
            restore();
        }
    }

private:
    // Whether `pos`, in global coordinates, lies on a visible, enabled button of a window's decoration.
    static bool overButton(const QPointF &pos)
    {
        KWin::Window *window = KWin::input()->findToplevel(pos);
        if (!window || !window->isDecorated()) {
            return false;
        }
        // The pointer is over the window's content, the common case, so no button can be under it.
        if (window->clientGeometry().contains(pos)) {
            return false;
        }
        // Decoration coordinates start at the frame's top left corner.
        const QPointF local = pos - window->frameGeometry().topLeft();
        const auto buttons = window->decoration()->findChildren<KDecoration3::DecorationButton *>();
        return std::any_of(buttons.cbegin(), buttons.cend(), [&local](const KDecoration3::DecorationButton *button) {
            return button->isVisible() && button->isEnabled() && button->geometry().contains(local);
        });
    }

    // Makes the hand the mouse cursor's source, remembering the source it replaces.
    void showHand()
    {
        KWin::Cursor *cursor = KWin::Cursors::self()->mouse();
        if (cursor->source() == m_hand.get()) {
            return;
        }
        m_previous = cursor->source();
        // The theme and size the rest of the session uses, read now so a theme change is followed.
        m_hand->setTheme(KWin::input()->pointer()->cursorTheme());
        m_hand->setShape(Qt::PointingHandCursor);
        cursor->setSource(m_hand.get());
        // POINTER_DEBUG=1 logs each time the hand appears.
        static const bool debug = qEnvironmentVariableIntValue("POINTER_DEBUG") == 1;
        if (debug) {
            qInfo("GGKWIN hand over a decoration button");
        }
    }

    // Gives the mouse cursor back to the source saved by showHand(), unless something else took it since.
    void restore()
    {
        KWin::Cursor *cursor = KWin::Cursors::self()->mouse();
        if (cursor->source() == m_hand.get() && m_previous) {
            cursor->setSource(m_previous);
        }
        m_previous = nullptr;
    }

    std::unique_ptr<KWin::ShapeCursorSource> m_hand;
    QPointer<KWin::CursorSource> m_previous;
};

// The factory KWin finds through the plugin metadata; it creates the spy when the plugin loads.
class PointingHandFactory : public KWin::PluginFactory
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID PluginFactory_iid FILE "metadata.json")
    Q_INTERFACES(KWin::PluginFactory)

public:
    std::unique_ptr<KWin::Plugin> create() const override
    {
        return std::make_unique<PointingHand>();
    }
};

} // namespace

#include "pointinghand.moc"
