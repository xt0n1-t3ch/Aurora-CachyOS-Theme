// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Draws the Aurora decoration in each state worth checking by eye (pointer over each button, a pressed
// button, inactive, maximized, a light theme, a dialog) on a blurred piece of the wallpaper, through
// kdecoration's own test bridge instead of KWin.
// Usage: deco-preview OUT.png WALLPAPER

#include "decoration.h"
#include "previewbridge.h"
#include "titlelayout.h"

#include <KDecoration3/DecorationButtonGroup>
#include <KDecoration3/DecorationSettings>

#include <QElapsedTimer>
#include <QFont>
#include <QGuiApplication>
#include <QHoverEvent>
#include <QImage>
#include <QPainter>

namespace {

constexpr int WINDOW_WIDTH = 760;
constexpr int CONTENT_STRIP = 34;
constexpr int LABEL_WIDTH = 330;
constexpr int GAP = 18;

// One row of the sheet: the state to draw and the label written beside it.
struct Scenario {
    const char *label;
    bool active = true;
    bool maximized = false;
    bool light = false;
    bool dialog = false;
    int hover = -1; ///< The button under the pointer, counted from the left; -1 for none.
    bool pressed = false;
};

QPalette paletteFor(bool light)
{
    // The window colors of the Aurora color scheme and its light counterpart ([Colors:Window]).
    const QColor window = light ? QColor(255, 255, 255) : QColor(36, 36, 36);
    const QColor text = light ? QColor(36, 36, 36) : QColor(222, 222, 222);
    QPalette palette;
    for (const QPalette::ColorGroup group : {QPalette::Active, QPalette::Inactive}) {
        palette.setColor(group, QPalette::Window, window);
        palette.setColor(group, QPalette::WindowText, text);
    }
    return palette;
}

// Runs the event loop for `milliseconds`, so animations and queued events finish before a capture.
void settle(int milliseconds)
{
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < milliseconds) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
    }
}

// Draws the title bar of one scenario over `backdrop` and returns the image, with a content strip below it.
QImage render(preview::Bridge &bridge, const std::shared_ptr<KDecoration3::DecorationSettings> &settings, const Scenario &scenario,
              const QImage &backdrop)
{
    preview::WindowState state;
    state.caption = scenario.dialog ? QStringLiteral("Delete permanently?") : QStringLiteral("xt0n1@XT0N1-PC: ~ — Konsole");
    state.palette = paletteFor(scenario.light);
    state.width = WINDOW_WIDTH;
    state.active = scenario.active;
    state.maximized = scenario.maximized;
    state.minimizeable = !scenario.dialog;
    state.maximizeable = !scenario.dialog;
    bridge.nextWindow = state;

    const QVariantMap bridgeArgument{{QStringLiteral("bridge"), QVariant::fromValue(static_cast<KDecoration3::DecorationBridge *>(&bridge))}};
    aurora::Decoration decoration(nullptr, QVariantList{bridgeArgument});
    decoration.setSettings(settings);
    decoration.create();
    decoration.init();
    decoration.apply(decoration.nextState()->clone());
    QObject::connect(&decoration, &KDecoration3::Decoration::nextStateChanged, &decoration,
                     [&decoration] { decoration.apply(decoration.nextState()->clone()); });
    settle(30);

    if (scenario.hover >= 0) {
        const auto group = decoration.findChild<KDecoration3::DecorationButtonGroup *>();
        QList<KDecoration3::DecorationButton *> visible;
        for (KDecoration3::DecorationButton *button : group->buttons()) {
            if (button->isVisible()) {
                visible.append(button);
            }
        }
        const QPointF pointer = visible.at(qMin(scenario.hover, int(visible.size()) - 1))->geometry().center();
        QHoverEvent move(QEvent::HoverMove, pointer, pointer, QPointF(-1, -1));
        QCoreApplication::sendEvent(&decoration, &move);
        if (scenario.pressed) {
            QMouseEvent press(QEvent::MouseButtonPress, pointer, pointer, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            QCoreApplication::sendEvent(&decoration, &press);
        }
        settle(220); // Longer than the 120 ms hover fade.
    }

    const QSize size(WINDOW_WIDTH, int(aurora::TITLE_BAR_HEIGHT) + CONTENT_STRIP);
    QImage image(size, QImage::Format_ARGB32_Premultiplied);
    QPainter painter(&image);
    painter.drawImage(QRect(QPoint(0, 0), size), backdrop);
    painter.fillRect(QRectF(0, aurora::TITLE_BAR_HEIGHT, size.width(), CONTENT_STRIP), state.palette.color(QPalette::Window));
    decoration.paint(&painter, QRectF(0, 0, size.width(), aurora::TITLE_BAR_HEIGHT));
    return image;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    if (argc != 3) {
        qCritical("usage: deco-preview OUT.png WALLPAPER");
        return 2;
    }
    // A cheap stand-in for KWin's blur: the wallpaper scaled far down and back up.
    const QImage wallpaper(QString::fromLocal8Bit(argv[2]));
    const QImage piece = wallpaper.copy(wallpaper.width() / 3, wallpaper.height() / 5, wallpaper.width() / 3, wallpaper.height() / 9);
    const QImage backdrop = piece.scaled(piece.width() / 28, piece.height() / 28, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                                .scaled(WINDOW_WIDTH, 120, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    preview::Bridge bridge;
    const auto settings = std::make_shared<KDecoration3::DecorationSettings>(&bridge);
    const QList<Scenario> scenarios{
        {"Activa"},
        {"Puntero en minimizar", true, false, false, false, 0},
        {"Puntero en maximizar", true, false, false, false, 1},
        {"Puntero en cerrar", true, false, false, false, 2},
        {"Cerrar presionado", true, false, false, false, 2, true},
        {"Inactiva", false},
        {"Inactiva, puntero en cerrar", false, false, false, false, 2},
        {"Maximizada (restaurar)", true, true, false, false, 1},
        {"Tema claro", true, false, true},
        {"Tema claro, puntero en cerrar", true, false, true, false, 2},
        {"Diálogo (solo cerrar)", true, false, false, true},
    };

    const int rowHeight = int(aurora::TITLE_BAR_HEIGHT) + CONTENT_STRIP;
    QImage sheet(LABEL_WIDTH + WINDOW_WIDTH + 2 * GAP, GAP + int(scenarios.size()) * (rowHeight + GAP), QImage::Format_ARGB32_Premultiplied);
    sheet.fill(QColor(20, 20, 23));
    QPainter painter(&sheet);
    QFont labelFont(QStringLiteral("SF Pro Text"));
    labelFont.setPixelSize(17);
    labelFont.setWeight(QFont::Medium);
    painter.setFont(labelFont);
    painter.setPen(QColor(235, 235, 240));
    int y = GAP;
    for (const Scenario &scenario : scenarios) {
        painter.drawText(QRect(GAP, y, LABEL_WIDTH - GAP, rowHeight), Qt::AlignVCenter | Qt::AlignLeft, QString::fromUtf8(scenario.label));
        painter.drawImage(QPoint(LABEL_WIDTH, y), render(bridge, settings, scenario, backdrop));
        y += rowHeight + GAP;
    }
    painter.end();
    return sheet.save(QString::fromLocal8Bit(argv[1])) ? 0 : 1;
}
