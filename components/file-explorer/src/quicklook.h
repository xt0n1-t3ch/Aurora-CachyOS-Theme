// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Quick Look: a glass window that shows a file without opening it, as Finder does on Space.

#pragma once

#include "glasswindow.h"

#include <KFileItem>

class QLabel;
class QPlainTextEdit;
class QStackedWidget;

namespace fe {

/// Shows one file: a picture whole, the start of a text, or a large preview with the file's kind, size and date.
/// Space or Esc closes it; Enter opens the file in its application.
class QuickLook : public GlassWindow
{
    Q_OBJECT

public:
    /// owner is the window Quick Look centers on.
    explicit QuickLook(QWidget *owner);
    /// Shows item, replacing what was shown, and raises the window.
    void showItem(const KFileItem &item);
    /// The item shown, or a null item.
    [[nodiscard]] KFileItem item() const;

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void showImage(const QString &path);
    void showText(const QString &path);
    void showPreview();
    /// Opens the item in its application and closes Quick Look.
    void openItem();
    /// Sizes the window for content of this size and centers it on the owner.
    void place(QSize content);

    QWidget *m_owner;
    KFileItem m_item;
    QLabel *m_title;
    QStackedWidget *m_pages;
    QLabel *m_image;
    QPlainTextEdit *m_text;
    QLabel *m_preview;
    QLabel *m_details;
};

} // namespace fe
