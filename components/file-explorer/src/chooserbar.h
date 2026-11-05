// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The button row at the bottom of an Open or Save panel.

#pragma once

#include <QStringList>
#include <QWidget>

class QComboBox;
class QLineEdit;
class QPushButton;

namespace fe {

/// The bottom of an Open or Save panel, as on macOS: "Save As" with the name (Save only), the file type popup
/// (when the app gives types), Cancel, and the default button on the right.
class ChooserBar : public QWidget
{
    Q_OBJECT

public:
    /// saving adds the name field; acceptLabel is the default button's text. The file type popup lists filterLabels
    /// with currentFilter chosen (a Save panel shows it only with more than one type). name is the starting name of a
    /// Save panel.
    ChooserBar(bool saving, const QString &acceptLabel, const QStringList &filterLabels, int currentFilter,
               const QString &name, QWidget *parent = nullptr);

    /// The typed name; empty in an Open panel.
    [[nodiscard]] QString name() const;
    /// Replaces the typed name; does nothing in an Open panel.
    void setName(const QString &name);
    /// The chosen file type; 0 when the bar has no file type popup.
    [[nodiscard]] int currentFilter() const;

Q_SIGNALS:
    /// The default button was clicked, or Return was pressed in the name field.
    void accepted();
    /// Cancel was clicked.
    void rejected();
    /// The user chose another file type.
    void filterChanged(int index);

private:
    QLineEdit *m_name = nullptr;
    QComboBox *m_filter = nullptr;
    QPushButton *m_accept = nullptr;
};

} // namespace fe
