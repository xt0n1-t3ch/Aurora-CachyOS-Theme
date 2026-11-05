// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Shared metrics, colors, fonts and painting helpers for the glass look.

#pragma once

#include <QColor>
#include <QFont>
#include <QPalette>
#include <QPixmap>
#include <QString>

class QPainter;
class QRectF;

/// Metrics and colors of the macOS 27 dark look, measured on 512pixels' Finder and
/// Settings screenshots and on the Folders app references. Sizes are in device-independent pixels.
namespace fe::style {

inline constexpr int WINDOW_RADIUS = 14;
inline constexpr int RESIZE_BORDER = 6;
inline constexpr int SIDEBAR_WIDTH = 236;
inline constexpr int SIDEBAR_INSET = 6;
inline constexpr int SIDEBAR_RADIUS = 12;
inline constexpr int TOOLBAR_HEIGHT = 52;
inline constexpr int CAPSULE_HEIGHT = 34;
inline constexpr int BUTTON_WIDTH = 38;
inline constexpr int SEARCH_WIDTH = 230;
inline constexpr int ROW_HEIGHT = 28;
/// A drive's row: its name over its capacity bar.
inline constexpr int DRIVE_ROW_HEIGHT = 38;
inline constexpr int SECTION_HEIGHT = 30;
inline constexpr int TABBAR_HEIGHT = 30;
inline constexpr int STATUS_HEIGHT = 28;
inline constexpr int PATHBAR_HEIGHT = 30;
inline constexpr int TOOL_ICON_SIZE = 18;
inline constexpr int SIDEBAR_ICON_SIZE = 17;
inline constexpr int MENU_RADIUS = 11;
inline constexpr int MENU_ITEM_HEIGHT = 24;
inline constexpr int MENU_SEPARATOR_HEIGHT = 11;

// The content area is the denser glass, for legibility; the sidebar floats as the clearer one.
inline const QColor WINDOW_TINT{24, 24, 27, 160};
inline const QColor WINDOW_TINT_INACTIVE{34, 34, 37, 150};
inline const QColor SIDEBAR_GLASS{56, 56, 62, 118};
inline const QColor SIDEBAR_GLASS_INACTIVE{62, 62, 66, 110};
inline const QColor WINDOW_RIM{255, 255, 255, 34};
inline const QColor MENU_FILL{36, 36, 40, 206};
inline const QColor MENU_SEPARATOR{255, 255, 255, 26};
inline const QColor SELECTION_INACTIVE{255, 255, 255, 38};
inline const QColor SELECTION_TILE{10, 132, 255, 96};
inline const QColor SELECTION_TILE_INACTIVE{255, 255, 255, 34};
inline const QColor SCROLLER{255, 255, 255, 92};
inline const QColor SCROLLER_HOVER{255, 255, 255, 150};
inline const QColor GLASS_RIM_TOP{255, 255, 255, 58};
inline const QColor GLASS_RIM{255, 255, 255, 26};
inline const QColor HAIRLINE{255, 255, 255, 20};
inline const QColor TEXT{255, 255, 255, 236};
inline const QColor TEXT_SECONDARY{235, 235, 245, 150};
inline const QColor TEXT_TERTIARY{235, 235, 245, 82};
inline const QColor ACCENT{10, 132, 255};
inline const QColor SELECTION{22, 118, 212};
inline const QColor HOVER{255, 255, 255, 20};
// The capacity bar of a drive: its track, and its fill once the drive is nearly full (macOS's system orange and red).
inline const QColor CAPACITY_TRACK{255, 255, 255, 36};
inline const QColor CAPACITY_LOW{255, 159, 10};
inline const QColor CAPACITY_CRITICAL{255, 69, 58};
inline const QColor CAPSULE_FILL{255, 255, 255, 22};
inline const QColor CAPSULE_HOVER{255, 255, 255, 30};
inline const QColor CAPSULE_PRESSED{255, 255, 255, 50};
inline const QColor SEGMENT_CHECKED{255, 255, 255, 46};

/// SF Pro (Text below 20 px, Display above), falling back to Inter and the system font.
[[nodiscard]] QFont uiFont(qreal pixelSize, QFont::Weight weight = QFont::Normal);

/// A theme icon drawn in one color, the way macOS draws its symbol images. Cached.
[[nodiscard]] QPixmap tintedIcon(const QString &name, int size, const QColor &color, qreal devicePixelRatio);

/// A Liquid Glass surface: a translucent fill and a rim that catches light at the top.
void paintGlass(QPainter &painter, const QRectF &rect, qreal radius, const QColor &fill);

/// The application palette: the macOS dark colors, for the windows that are not glass (dialogs).
[[nodiscard]] QPalette darkPalette();

} // namespace fe::style
