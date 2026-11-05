// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Tests the title bar geometry of titlelayout.h: the capsule, the hit areas and the caption.

#include "titlelayout.h"

#include <QRandomGenerator>
#include <QTest>

using namespace aurora;

class TitleLayoutTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void theButtonsShareOneCapsuleAtTheRightEdge()
    {
        const TitleLayout layout = layoutTitleBar(1000, TITLE_BAR_HEIGHT, 3, false);

        // Three 38 px segments plus 3 px of padding at each end, 8 px from the window edge,
        // vertically centred in the 46 px bar: the capsule of the File Explorer's toolbar.
        QCOMPARE(layout.capsule, QRectF(1000 - 8 - 120, 6, 120, 34));
        QCOMPARE(layout.buttons.size(), 3);
        QCOMPARE(layout.buttons.at(0), QRectF(875, 6, 38, 34));
        QCOMPARE(layout.buttons.at(2), QRectF(951, 6, 38, 34));
    }

    void aWindowThatCannotMaximizeGetsATwoButtonCapsule()
    {
        const TitleLayout layout = layoutTitleBar(1000, TITLE_BAR_HEIGHT, 2, false);

        QCOMPARE(layout.capsule.width(), 82.0);
        QCOMPARE(layout.capsule.right(), 992.0);
    }

    void noButtonsMeansNoCapsule()
    {
        const TitleLayout layout = layoutTitleBar(1000, TITLE_BAR_HEIGHT, 0, false);

        QVERIFY(layout.capsule.isEmpty());
        QVERIFY(layout.buttons.isEmpty());
        QVERIFY(layout.hitAreas.isEmpty());
    }

    void aFloatingWindowIsClickedExactlyWhereItsButtonsAreDrawn()
    {
        const TitleLayout layout = layoutTitleBar(1000, TITLE_BAR_HEIGHT, 3, false);

        QCOMPARE(layout.hitAreas, layout.buttons);
    }

    void aMaximizedWindowsButtonsReachTheScreenCorner()
    {
        const TitleLayout layout = layoutTitleBar(1000, TITLE_BAR_HEIGHT, 3, true);

        // Throwing the pointer into the top right corner closes a maximized window, as on Windows.
        QCOMPARE(layout.hitAreas.last().topRight(), QPointF(1000, 0));
        for (qsizetype i = 0; i < layout.hitAreas.size(); ++i) {
            QCOMPARE(layout.hitAreas.at(i).top(), 0.0);
            QVERIFY(layout.hitAreas.at(i).contains(layout.buttons.at(i)));
        }
        // The group lays the buttons out one after the other, so the hit areas must touch.
        QCOMPARE(layout.hitAreas.at(1).left(), layout.hitAreas.at(0).right());
        QCOMPARE(layout.hitAreas.at(2).left(), layout.hitAreas.at(1).right());
    }

    void aShortTitleIsCentredOnTheWindow()
    {
        const TitleLayout layout = layoutTitleBar(1000, TITLE_BAR_HEIGHT, 3, false);

        QCOMPARE(captionRect(1000, TITLE_BAR_HEIGHT, 200, layout.capsule), QRectF(400, 0, 200, TITLE_BAR_HEIGHT));
    }

    void aLongTitleMovesLeftInsteadOfRunningUnderTheButtons()
    {
        const TitleLayout layout = layoutTitleBar(1000, TITLE_BAR_HEIGHT, 3, false);

        // Centred it would end at 880, past the capsule's left edge (872) minus the 12 px gap.
        QCOMPARE(captionRect(1000, TITLE_BAR_HEIGHT, 760, layout.capsule), QRectF(100, 0, 760, TITLE_BAR_HEIGHT));
    }

    void aTitleWiderThanTheBarGetsAllTheSpaceLeft()
    {
        const TitleLayout layout = layoutTitleBar(1000, TITLE_BAR_HEIGHT, 3, false);

        QCOMPARE(captionRect(1000, TITLE_BAR_HEIGHT, 2000, layout.capsule), QRectF(12, 0, 848, TITLE_BAR_HEIGHT));
    }

    void theTitleNeverTouchesTheButtonsAndStaysCentredWhenItCan()
    {
        // A random seed covers many sizes; it is printed on failure so a run can be reproduced.
        const quint32 seed = QRandomGenerator::global()->generate();
        QRandomGenerator random(seed);
        for (int run = 0; run < 2000; ++run) {
            const qreal width = 200 + random.bounded(3000);
            const int buttons = static_cast<int>(random.bounded(4));
            const qreal text = random.bounded(2500);
            const TitleLayout layout = layoutTitleBar(width, TITLE_BAR_HEIGHT, buttons, random.bounded(2) == 1);
            const QRectF caption = captionRect(width, TITLE_BAR_HEIGHT, text, layout.capsule);
            const QByteArray where = "seed " + QByteArray::number(seed) + ", width " + QByteArray::number(width) + ", buttons "
                + QByteArray::number(buttons) + ", text " + QByteArray::number(text);

            QVERIFY2(caption.left() >= CAPTION_GAP - 1e-9, where.constData());
            const qreal limit = layout.capsule.isEmpty() ? width - CAPTION_GAP : layout.capsule.left() - CAPTION_GAP;
            QVERIFY2(caption.right() <= limit + 1e-9, where.constData());
            QVERIFY2(caption.width() <= text + 1e-9, where.constData());
            const bool fitsCentred = (width - text) / 2 >= CAPTION_GAP && (width + text) / 2 <= limit;
            if (fitsCentred) {
                QVERIFY2(qAbs(caption.center().x() - width / 2) < 1e-9, where.constData());
            }
        }
    }
};

QTEST_GUILESS_MAIN(TitleLayoutTest)
#include "test_titlelayout.moc"
