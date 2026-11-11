// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The address field of the toolbar: it shows where the tab is as clickable segments and takes a typed path, with
// suggestions and Tab.

#pragma once

#include "core/paths.h"

#include <QLineEdit>
#include <QUrl>

class QCompleter;
class QStandardItemModel;
class QTimer;

namespace fe {

class CrumbStrip;

/// A glass field with the folder's path. At rest the path shows as segments (fe::CrumbStrip) that open, list their
/// folders and take drops, as the address bar of Windows' File Explorer does. A click beside them, Ctrl+L or Alt+D
/// selects the path for typing; the subfolders of what is typed drop down below, Tab completes like a shell, Enter
/// goes there and Esc puts the segments back.
class AddressField : public QLineEdit
{
    Q_OBJECT

public:
    /// homePath is where a typed "~" leads.
    explicit AddressField(QString homePath, QWidget *parent = nullptr);

    /// Shows url's address (fe::addressText), unless the user is typing; typed paths start at url's folder.
    void setLocation(const QUrl &url);
    /// The segments shown at rest, from fe::crumbsFor.
    void setCrumbs(const QList<Crumb> &crumbs);
    /// The segments, whose signals say what the user chose.
    [[nodiscard]] CrumbStrip *crumbs() const;
    /// Takes the keyboard with the whole path selected.
    void startEditing();
    /// Rings the field red for a moment: the typed path leads nowhere. The text stays for correcting.
    void showError();

Q_SIGNALS:
    /// Enter was pressed, or a suggestion chosen, with this text.
    void addressEntered(const QString &text);
    /// Esc gave up typing; the field shows the path again.
    void editingCancelled();

protected:
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    /// The names of the subfolders of the folder the text is typed into; empty when that is no local folder.
    [[nodiscard]] QStringList subfolderNames(const QString &text) const;
    void refreshSuggestions();
    void showLocation();
    /// The room the segments have: the field between its icon and its right margin.
    void placeCrumbs();

    QString m_homePath;
    QUrl m_location;
    /// Each suggestion shows the folder's name and completes to its whole path (PATH_ROLE).
    QStandardItemModel *m_suggestions;
    QCompleter *m_completer;
    QTimer *m_errorTimer;
    CrumbStrip *m_crumbs;
};

} // namespace fe
