// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The application style that draws menus, tooltips, scroll bars, list selection and column headers the Finder way.

#pragma once

#include <QProxyStyle>

namespace fe {

/// The application style: the platform style underneath, with the parts macOS 27 draws differently
/// replaced: glass menus and tooltips with blur and shadow, rounded item selection (Dolphin draws
/// its selection through PE_PanelItemViewItem when the style is not Breeze, kitemlistwidget.cpp,
/// v26.08.2, line 676), thin scroll bars without arrows, and flat column headers.
class FinderStyle : public QProxyStyle
{
    Q_OBJECT

public:
    explicit FinderStyle(QStyle *base);

    using QProxyStyle::polish;
    using QProxyStyle::unpolish;
    /// Makes menus and tooltips glass popups and gives buttons the hand cursor.
    void polish(QWidget *widget) override;
    void unpolish(QWidget *widget) override;

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget = nullptr) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget = nullptr) const override;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter,
                            const QWidget *widget = nullptr) const override;
    [[nodiscard]] QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl,
                                       const QWidget *widget = nullptr) const override;
    [[nodiscard]] SubControl hitTestComplexControl(ComplexControl control, const QStyleOptionComplex *option,
                                                   const QPoint &pos, const QWidget *widget = nullptr) const override;
    [[nodiscard]] int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                                  const QWidget *widget = nullptr) const override;
    [[nodiscard]] QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                                         const QWidget *widget = nullptr) const override;

protected:
    /// Blurs and shadows a glass popup each time it shows or resizes.
    bool eventFilter(QObject *watched, QEvent *event) override;
};

} // namespace fe
