// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// svgcheck OUT.png SIZE FILE.svg...: renders each SVG with QtSvg (the renderer Plasma uses for icons)
// side by side on a dark tile, so an icon is judged the way the Dock will draw it.
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>

// Returns 0 when OUT.png is written, 1 when an SVG is invalid or the PNG cannot be saved, and 2 on missing arguments.
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    if (argc < 4) {
        qCritical("usage: svgcheck OUT.png SIZE FILE.svg...");
        return 2;
    }
    const int size = QString::fromLocal8Bit(argv[2]).toInt();
    const int count = argc - 3;
    // The tiles sit in one row, with a padding of an eighth of SIZE around and between them.
    const int pad = size / 8;
    QImage out(pad + count * (size + pad), size + 2 * pad, QImage::Format_ARGB32_Premultiplied);
    out.fill(QColor(36, 40, 46));
    QPainter painter(&out);
    for (int i = 0; i < count; ++i) {
        QSvgRenderer svg(QString::fromLocal8Bit(argv[3 + i]));
        if (!svg.isValid()) {
            qCritical("invalid svg: %s", argv[3 + i]);
            return 1;
        }
        svg.render(&painter, QRectF(pad + i * (size + pad), pad, size, size));
    }
    painter.end();
    return out.save(QString::fromLocal8Bit(argv[1])) ? 0 : 1;
}
