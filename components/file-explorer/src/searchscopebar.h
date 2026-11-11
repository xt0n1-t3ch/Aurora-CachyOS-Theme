// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The scope row of a search, as Finder shows it under its toolbar: search the whole PC or only the folder the search
// started in.

#pragma once

#include <QWidget>

namespace fe {

/// Two capsules, "This PC" and the folder's name, the chosen one filled, and a Save capsule at the right that keeps the
/// search as a smart folder in the sidebar. Shown only while a search of the index runs; the folder's capsule only when
/// the index holds the folder.
class SearchScopeBar : public QWidget
{
    Q_OBJECT

public:
    explicit SearchScopeBar(QWidget *parent = nullptr);
    /// The folder's name on the second capsule, and which capsule is chosen.
    void setScope(const QString &folderName, bool everywhere);
    /// Whether the folder's capsule shows: the index must hold the folder to search only there.
    void setFolderShown(bool shown);

Q_SIGNALS:
    /// The user chose a scope.
    void scopeChosen(bool everywhere);
    /// The user pressed Save.
    void saveRequested();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    /// The capsule's rectangle: 0 This PC, 1 the folder, 2 Save.
    [[nodiscard]] QRectF capsule(int which) const;

    QString m_folderName;
    bool m_everywhere = true;
    bool m_folderShown = true;
};

} // namespace fe
