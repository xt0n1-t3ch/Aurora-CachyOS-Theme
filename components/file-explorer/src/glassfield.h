// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// A text field drawn as a small glass capsule, for the rows and panels inside the window (the filter row, the
// Inspector). The toolbar's fields draw their own, larger capsules.

#pragma once

#include <QLineEdit>

namespace fe {

/// A frameless line edit on a glass capsule with an optional symbol at its left and the accent ring while it has the
/// keyboard. Its own background stays clear, so the window's glass shows through.
class GlassLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    /// iconName is a theme icon drawn at the left, or empty for none. height is the capsule's in pixels.
    explicit GlassLineEdit(const QString &iconName, int height, QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_iconName;
};

} // namespace fe
