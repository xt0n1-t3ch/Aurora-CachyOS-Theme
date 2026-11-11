// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The filter row of a tab: as each key is typed, the view keeps only the items whose names contain the text. It sits
// where the search scope row does, under the toolbar, and belongs to the folder on screen.

#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;

namespace fe {

class GlassButton;

/// A glass field, the number of matching items and a close button. Escape closes it; Enter or the Down arrow hand the
/// keyboard back to the view, so the first match can be opened without the mouse.
class FilterBar : public QWidget
{
    Q_OBJECT

public:
    explicit FilterBar(QWidget *parent = nullptr);

    /// Shows the row and gives its field the keyboard with any text selected.
    void open();
    /// Empties the field and hides the row. Sends filterChanged with an empty text when the field held one.
    void dismiss();
    [[nodiscard]] QString text() const;
    /// How many items the view shows with the filter; shown only while the field holds text.
    void setMatchCount(qsizetype matches);
    /// The count as shown, for tests and assistive technology.
    [[nodiscard]] QString matchText() const;

Q_SIGNALS:
    /// The text changed; an empty text shows every item again.
    void filterChanged(const QString &text);
    /// Enter or the Down arrow: the view should take the keyboard.
    void focusViewRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QLineEdit *m_field;
    QLabel *m_count;
    GlassButton *m_close;
};

} // namespace fe
