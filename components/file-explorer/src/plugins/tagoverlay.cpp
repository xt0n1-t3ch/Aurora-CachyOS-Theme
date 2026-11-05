// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// A KIO overlay icon plugin: file views (File Explorer's and Dolphin's) draw a tagged file's color dots on the corner
// of its icon. The emblems come from the Aurora icon theme (themes/icons/tag_emblems.py).

#include "core/tags.h"

#include <KFileMetaData/UserMetaData>
#include <KOverlayIconPlugin>

class TagOverlayPlugin : public KOverlayIconPlugin
{
    Q_PLUGIN_METADATA(IID "org.kde.overlayicon.aurora-tags" FILE "tagoverlay.json")
    Q_OBJECT

public:
    /// The emblem of the file's color tags, or nothing for an untagged file or a place that is not local.
    QStringList getOverlays(const QUrl &url) override
    {
        if (!url.isLocalFile()) {
            return {};
        }
        const QString emblem = fe::tagEmblem(KFileMetaData::UserMetaData(url.toLocalFile()).tags());
        return emblem.isEmpty() ? QStringList() : QStringList{emblem};
    }
};

#include "tagoverlay.moc"
