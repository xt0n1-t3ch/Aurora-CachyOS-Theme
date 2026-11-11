// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Compare Folders: the two folders of Split View side by side as one list of what differs, with copies across to bring
// them in step. The rules are in core/compare.

#pragma once

#include "core/compare.h"

#include <QDialog>
#include <QUrl>

#include <memory>

class QCheckBox;
class QLabel;
class QPushButton;
class QTreeWidget;

namespace fe {

/// A dialog that walks both folders on a thread at idle priority, lists every path that is only on one side or
/// differs (and, when asked, the ones that are the same), and copies the selected ones to the left or the right with
/// KIO, recorded for Undo. It compares again after each copy.
class CompareSheet : public QDialog
{
    Q_OBJECT

public:
    CompareSheet(const QUrl &left, const QUrl &right, QWidget *parent = nullptr);
    ~CompareSheet() override;

    /// The rows of the last comparison.
    [[nodiscard]] const QList<CompareRow> &rows() const;

Q_SIGNALS:
    /// A comparison finished and its rows are shown.
    void compared();

private:
    void compare();
    void showRows(const QList<CompareRow> &rows);
    /// Copies the selected rows from one side to the other: true copies left to right.
    void copyAcross(bool toRight);

    QString m_left;
    QString m_right;
    QList<CompareRow> m_rows;
    std::shared_ptr<std::atomic_bool> m_cancel;
    QTreeWidget *m_list;
    QLabel *m_summary;
    QCheckBox *m_contents;
    QCheckBox *m_showSame;
    QPushButton *m_toRight;
    QPushButton *m_toLeft;
};

} // namespace fe
