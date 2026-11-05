/*
    SPDX-FileCopyrightText: 2014 Marco Martin <mart@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "view.h"

#include <QAction>
#include <QClipboard>
#include <QDebug>
#include <QGuiApplication>
#include <QPlatformSurfaceEvent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QScreen>

#include <KAuthorized>
#include <KCrash>
#include <KIO/CommandLauncherJob>
#include <KLocalizedString>
#include <KService>
#include <KWindowEffects>
#include <KWindowSystem>

#include <PlasmaQuick/PlasmaQuick>

#include <LayerShellQt/Window>
#include <algorithm>
#include <qnamespace.h>

#include "appadaptor.h"

KCONFIGGROUP_DECLARE_ENUM_QOBJECT(View, HistoryBehavior)

// Aurora: a frame prefix the theme does not have, so PlasmaWindow draws no frame, margins or shadow; the glass
// panel and its shadow are drawn by RunCommand.qml, as macOS Tahoe's Spotlight is one floating piece of glass.
View::View(QWindow *)
    : PlasmaQuick::PlasmaWindow(u"aurora/no-frame"_s)
    , m_engine(PlasmaQuick::globalEngine())
    , m_floating(false)
{
    KCrash::initialize();

    // used only by screen readers
    setTitle(i18n("KRunner"));

    m_config = KConfigGroup(KSharedConfig::openConfig(), u"General"_s);
    m_stateData = KSharedConfig::openConfig(u"krunnerstaterc"_s, //
                                            KConfig::NoGlobals,
                                            QStandardPaths::GenericDataLocation)
                      ->group(u"General"_s);
    m_configWatcher = KConfigWatcher::create(KSharedConfig::openConfig());
    connect(m_configWatcher.data(), &KConfigWatcher::configChanged, this, [this](const KConfigGroup &group) {
        const QLatin1String pluginsGrp("Plugins");
        if (group.name() == QLatin1String("General")) {
            loadConfig();
        } else if (group.name() == pluginsGrp) {
            Q_EMIT helpEnabledChanged();
        }
    });
    connect(&m_consumer, &KActivities::Consumer::currentActivityChanged, this, &View::activityChanged);
    Q_EMIT activityChanged(m_consumer.currentActivity());

    loadConfig();

    new AppAdaptor(this);
    QDBusConnection::sessionBus().registerObject(u"/App"_s, this);

    QQmlComponent component(m_engine.get(), u"org.kde.krunner.private.view"_s, u"RunCommand"_s);
    QVariantMap properties{{u"runnerWindow"_s, QVariant::fromValue(this)}};
    auto obj = component.createWithInitialProperties(properties);

    auto item = qobject_cast<QQuickItem *>(obj);
    setMainItem(item);

    auto updateSize = [this]() {
        resize(QSize(mainItem()->implicitWidth(), mainItem()->implicitHeight()).grownBy(padding()).boundedTo(screen()->availableSize().shrunkBy(margins())));
    };

    connect(item, &QQuickItem::implicitHeightChanged, this, updateSize);
    connect(this, &View::paddingChanged, this, updateSize);
    updateSize();

    auto screenRemoved = [this](QScreen *screen) {
        if (screen == this->screen()) {
            setScreen(qGuiApp->primaryScreen());
            hide();
        }
    };

    connect(qGuiApp, &QGuiApplication::screenRemoved, this, screenRemoved);
    connect(qGuiApp, &QGuiApplication::focusWindowChanged, this, &View::slotFocusWindowChanged);
}

View::~View()
{
    delete mainItem();
}

QMargins View::margins()
{
    if (m_floating) {
        // A quarter of the way down, where macOS puts Spotlight: the Applications view grows downwards from there.
        const QRect r = screen()->availableGeometry();
        return QMargins({0, r.height() / 4, 0, 0});
    } else {
        return {}; // Zeros
    }
}

void View::objectIncubated()
{
}

void View::slotFocusWindowChanged()
{
    if (QGuiApplication::focusWindow() && m_requestedClipboardSelection) {
        displayWithClipboardContents();
    }

    if (!QGuiApplication::focusWindow() && !m_pinned) {
        setVisible(false);
    }
}

bool View::freeFloating() const
{
    return m_floating;
}

void View::setFreeFloating(bool floating)
{
    m_floating = floating;
    if (m_floating) {
        KWindowEffects::slideWindow(this, KWindowEffects::NoEdge);
        setBorders(Qt::LeftEdge | Qt::TopEdge | Qt::RightEdge | Qt::BottomEdge);
    } else {
        KWindowEffects::slideWindow(this, KWindowEffects::TopEdge);
        setBorders(Qt::LeftEdge | Qt::RightEdge | Qt::BottomEdge);
    }

    positionOnScreen();
}

void View::loadConfig()
{
    // Quick Menu floats on the active screen unless KRunner's settings put it at the top edge.
    setFreeFloating(m_config.readEntry("FreeFloating", true));
    // Quick Menu opens on the Applications view; KRunner's "Retain previous search" setting still brings it back.
    setRetainPriorSearch(m_config.readEntry("RetainPriorSearch", false));
    setPinned(m_stateData.readEntry("Pinned", false));
    setHistoryBehavior(m_config.readEntry("historyBehavior", HistoryBehavior::CompletionSuggestion));
}

void View::showEvent(QShowEvent *event)
{
    QQuickWindow::showEvent(event);
    requestActivate();
}

QRectF View::inputRect() const
{
    return m_inputRect;
}

void View::setInputRect(const QRectF &rect)
{
    if (m_inputRect == rect) {
        return;
    }
    m_inputRect = rect;
    setMask(QRegion(rect.toAlignedRect()));
    Q_EMIT inputRectChanged();
}

void View::resizeEvent(QResizeEvent *event)
{
    PlasmaQuick::PlasmaWindow::resizeEvent(event);
    // PlasmaWindow resets the mask from its (empty) frame when the window changes size.
    if (!m_inputRect.isEmpty()) {
        setMask(QRegion(m_inputRect.toAlignedRect()));
    }
}

void View::positionOnScreen()
{
    const auto screens = QGuiApplication::screens();
    if (m_floating) {
        auto message = QDBusMessage::createMethodCall(u"org.kde.KWin"_s, u"/KWin"_s, u"org.kde.KWin"_s, u"activeOutputName"_s);
        QDBusReply<QString> reply = QDBusConnection::sessionBus().call(message);
        if (reply.isValid()) {
            const QString activeOutputName = reply.value();
            const auto screenIt = std::ranges::find_if(screens, [&activeOutputName](QScreen *screen) {
                return screen->name() == activeOutputName;
            });
            if (screenIt != screens.end()) {
                setScreen(*screenIt);
            }
        }
    }

    auto layerWindow = LayerShellQt::Window::get(this);
    layerWindow->setAnchors(LayerShellQt::Window::AnchorTop);
    layerWindow->setLayer(LayerShellQt::Window::LayerTop);
    layerWindow->setScope(u"krunner"_s);
    layerWindow->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityOnDemand);
    layerWindow->setMargins(margins());
    if (m_floating) {
        layerWindow->setScreen(screen());
    } else {
        layerWindow->setWantsToBeOnActiveScreen(true);
    }
}

void View::toggleDisplay()
{
    if (isVisible()) {
        setVisible(false);
    } else {
        display();
    }
}

void View::display()
{
    positionOnScreen();
    setVisible(true);
}

void View::displaySingleRunner(const QString &runnerName)
{
    display();

    mainItem()->setProperty("singleRunner", runnerName);
    mainItem()->setProperty("query", QString());
}

void View::displayWithClipboardContents()
{
    display();

    // On Wayland we cannot retrieve the clipboard selection until we get the focus
    if (QGuiApplication::focusWindow()) {
        m_requestedClipboardSelection = false;
        mainItem()->setProperty("singleRunner", QString());
        mainItem()->setProperty("query", QGuiApplication::clipboard()->text(QClipboard::Selection));
    } else {
        m_requestedClipboardSelection = true;
    }
}

void View::query(const QString &term)
{
    display();

    mainItem()->setProperty("singleRunner", QString());
    mainItem()->setProperty("query", term);
}

void View::querySingleRunner(const QString &runnerName, const QString &term)
{
    display();

    mainItem()->setProperty("singleRunner", runnerName);
    mainItem()->setProperty("query", term);
}

bool View::pinned() const
{
    return m_pinned;
}

void View::setPinned(bool pinned)
{
    if (m_pinned != pinned) {
        m_pinned = pinned;
        m_stateData.writeEntry("Pinned", pinned);
        Q_EMIT pinnedChanged();
    }
}

#include "moc_view.cpp"
