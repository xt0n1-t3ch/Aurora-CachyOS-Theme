// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The gallery view's stage: the selected item, as large as the view allows, above Dolphin's icons, which become the
// gallery's strip of thumbnails.

#pragma once

#include <KFileItem>

#include <QPixmap>
#include <QPointer>
#include <QWidget>

class KJob;
class QTimer;

namespace fe {

/// A picture of the selected item: its preview where a thumbnailer knows its kind (pictures, videos, documents,
/// fonts), else its icon, with its name and kind under it. It asks for a preview only while shown, and drops a preview
/// that arrives for an item no longer selected.
class GalleryStage : public QWidget
{
    Q_OBJECT

public:
    explicit GalleryStage(QWidget *parent = nullptr);

    /// Shows item; a null item shows a hint to select one.
    void setItem(const KFileItem &item);
    [[nodiscard]] KFileItem item() const;
    /// Whether the picture shown is the item's own preview rather than its icon.
    [[nodiscard]] bool showsPreview() const;

Q_SIGNALS:
    /// The picture of the item on stage changed (its preview arrived).
    void pictureChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void requestPreview();

    KFileItem m_item;
    QPixmap m_picture;
    bool m_preview = false;
    QPointer<KJob> m_job;
    /// Gathers quick changes of selection (arrow keys held down) into one preview request.
    QTimer *m_request;
};

} // namespace fe
