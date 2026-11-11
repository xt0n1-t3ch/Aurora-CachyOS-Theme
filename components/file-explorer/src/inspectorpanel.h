// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// The Inspector, a panel at the right of the files, as Finder's: what the selection is, how big it really is (folders
// included, kept current while it is open), its dates, owner and permissions, and the hash of a file to check a
// download against. The rules it reads by are in core/inspector.

#pragma once

#include "core/inspector.h"

#include <KFileItem>

#include <QPointer>
#include <QWidget>

#include <memory>

class KDirWatch;
class KJob;
class QComboBox;
class QFormLayout;
class QLabel;
class QThread;
class QTimer;

namespace fe {

class GlassButton;
class GlassLineEdit;

/// A sheet of glass beside the view. It works only while it is shown: hidden, it holds no watch and runs nothing.
/// Folder sizes are counted on a thread at idle priority and counted again when what the folder lists changes; a
/// hash is computed only when asked for.
class InspectorPanel : public QWidget
{
    Q_OBJECT

public:
    explicit InspectorPanel(QWidget *parent = nullptr);
    ~InspectorPanel() override;

    /// Shows items, or the folder when items is empty. Nothing restarts when they are the items shown already.
    void setItems(const KFileItemList &items, const QUrl &folder);
    /// The size line as shown, for tests and assistive technology.
    [[nodiscard]] QString sizeText() const;
    /// The hash shown, empty until one is computed.
    [[nodiscard]] QString hashText() const;
    /// The verdict of the compare field, as shown.
    [[nodiscard]] QString compareText() const;

Q_SIGNALS:
    /// The size line reached its final value for the items on screen.
    void sizeCounted();
    /// A hash finished.
    void hashComputed();

protected:
    void paintEvent(QPaintEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    /// Fills the rows from m_items and starts the count.
    void refresh();
    void showPreview();
    void startCount();
    void startHash();
    void updateCompare();
    /// Stops the count and the hash, drops their late results, and lets go of every watch.
    void stopWork();
    [[nodiscard]] QStringList localPaths() const;

    KFileItemList m_items;
    QUrl m_folder;
    QList<QUrl> m_shownUrls;
    /// Set to stop the running count or hash; each run has its own, so a late result of an old run is dropped.
    std::shared_ptr<std::atomic_bool> m_countCancel;
    std::shared_ptr<std::atomic_bool> m_hashCancel;
    QList<QPointer<QThread>> m_threads;
    /// The KIO job that reads the folder on screen or counts items that are not on this PC.
    QPointer<KJob> m_job;
    KDirWatch *m_watch;
    QTimer *m_recount;

    QLabel *m_preview;
    QLabel *m_name;
    QLabel *m_kind;
    QFormLayout *m_rows;
    QLabel *m_size;
    QWidget *m_hashBox;
    QComboBox *m_hashKind;
    GlassButton *m_hashButton;
    QLabel *m_hash;
    /// The computed hash in hex, as copied and compared; the label shows it in breakable groups.
    QString m_hashHex;
    GlassButton *m_copyHash;
    GlassLineEdit *m_compare;
    QLabel *m_verdict;
};

} // namespace fe
