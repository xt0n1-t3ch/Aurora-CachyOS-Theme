// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The address field of the toolbar: it shows where the tab is and takes a typed path, with suggestions and Tab.

#pragma once

#include <QLineEdit>
#include <QUrl>

class QCompleter;
class QStandardItemModel;
class QTimer;

namespace fe {

/// A glass field with the folder's path. A click, Ctrl+L or Alt+D selects the path for typing; the subfolders of
/// what is typed drop down below, Tab completes like a shell, Enter goes there and Esc puts the path back.
class AddressField : public QLineEdit
{
    Q_OBJECT

public:
    /// homePath is where a typed "~" leads.
    explicit AddressField(QString homePath, QWidget *parent = nullptr);

    /// Shows url's address (fe::addressText), unless the user is typing; typed paths start at url's folder.
    void setLocation(const QUrl &url);
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

private:
    /// The names of the subfolders of the folder the text is typed into; empty when that is no local folder.
    [[nodiscard]] QStringList subfolderNames(const QString &text) const;
    void refreshSuggestions();
    void showLocation();

    QString m_homePath;
    QUrl m_location;
    /// Each suggestion shows the folder's name and completes to its whole path (PATH_ROLE).
    QStandardItemModel *m_suggestions;
    QCompleter *m_completer;
    QTimer *m_errorTimer;
};

} // namespace fe
