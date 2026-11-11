// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The Rename sheet for several items: Finder's rules (replace text, add text, a format with numbers or dates) and a
// change of case, with every new name shown before anything is renamed. The rules are in core/batchrename.

#pragma once

#include "core/batchrename.h"

#include <KFileItem>

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTreeWidget;

namespace fe {

/// A dialog that renames items with one KIO batch job, so one Undo takes the whole batch back. The preview marks
/// every name that cannot be taken and why, and Rename stays off until none is left.
class BatchRenameSheet : public QDialog
{
    Q_OBJECT

public:
    /// items are renamed in this order, which numbers follow. Names in their folders are read when the sheet opens
    /// and again just before renaming.
    explicit BatchRenameSheet(const KFileItemList &items, QWidget *parent = nullptr);

    /// The plan the sheet shows now.
    [[nodiscard]] const RenamePlan &plan() const;
    /// The rule the controls describe.
    [[nodiscard]] RenameRule rule() const;
    /// Sets the controls to rule, for tests and for the sheet's remembered choice.
    void setRule(const RenameRule &rule);

Q_SIGNALS:
    /// The batch finished; renamed holds the new URLs of the items that were renamed.
    void renamed(const QList<QUrl> &renamed);

protected:
    void accept() override;

private:
    void buildControls();
    /// Reads the names in each item's folder, for the check against names already there.
    void readFolders();
    /// Plans again from the controls and refreshes the preview and the Rename button.
    void refresh();

    KFileItemList m_items;
    QList<RenameItem> m_renameItems;
    QHash<QString, QSet<QString>> m_namesInFolders;
    RenamePlan m_plan;
    bool m_settingRule = false;

    QComboBox *m_mode;
    QStackedWidget *m_pages;
    QLineEdit *m_find;
    QLineEdit *m_replace;
    QCheckBox *m_matchCase;
    QCheckBox *m_regex;
    QLineEdit *m_addText;
    QComboBox *m_addWhere;
    QComboBox *m_format;
    QComboBox *m_formatWhere;
    QLineEdit *m_customName;
    QSpinBox *m_startAt;
    QComboBox *m_case;
    QTreeWidget *m_preview;
    QLabel *m_summary;
    QPushButton *m_renameButton;
};

} // namespace fe
