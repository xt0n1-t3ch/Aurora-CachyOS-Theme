// SPDX-FileCopyrightText: 2026 xt0n1 (https://github.com/xt0n1-t3ch)
// SPDX-License-Identifier: GPL-3.0-only
// Additional terms (GPL-3.0 section 7, author attribution): see NOTICE.md.
// Tests the Energy page's arithmetic and sysfs reading: watts from RAPL counters (wrap included), the package zone
// of a powercap tree, and the bounded history the chart draws. Run: build/test_power
#include "power.h"

#include <QTemporaryDir>
#include <QTest>

#include <cmath>

namespace {

// Writes `text` to `path`, creating the directories above it.
void write(const QString &path, const QByteArray &text)
{
    QDir().mkpath(QFileInfo(path).path());
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(text);
}

} // namespace

class TestPower : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void five_joules_in_one_second_is_five_watts()
    {
        QCOMPARE(energy::raplWatts(1'000'000, 6'000'000, 65'532'610'987, 1.0), 5.0);
    }

    void a_counter_that_wrapped_still_gives_the_energy_since_the_last_reading()
    {
        // 1 J before the wrap and 1 J after it, in half a second: 4 W.
        QCOMPARE(energy::raplWatts(9'000'000, 1'000'000, 10'000'000, 0.5), 4.0);
    }

    void no_elapsed_time_gives_no_reading()
    {
        QVERIFY(std::isnan(energy::raplWatts(1'000, 2'000, 10'000, 0.0)));
        QVERIFY(std::isnan(energy::raplWatts(1'000, 2'000, 10'000, -1.0)));
    }

    void the_package_zone_is_found_by_its_name_not_its_position()
    {
        QTemporaryDir root;
        // The shape of an AMD Zen 3 tree: a package zone and a core subzone below it, which only has a name.
        write(root.filePath(QStringLiteral("intel-rapl:0:0/name")), "core\n");
        write(root.filePath(QStringLiteral("intel-rapl:0/name")), "package-0\n");
        write(root.filePath(QStringLiteral("intel-rapl:0/max_energy_range_uj")), "65532610987\n");
        write(root.filePath(QStringLiteral("intel-rapl:0/energy_uj")), "123\n");
        const auto zone = energy::findPackageZone(root.path());
        QVERIFY(zone.has_value());
        QCOMPARE(zone->counter, root.filePath(QStringLiteral("intel-rapl:0/energy_uj")));
        QCOMPARE(zone->range, quint64(65'532'610'987));
        QCOMPARE(energy::readNumber(zone->counter), std::optional<quint64>(123));
    }

    void a_tree_without_a_package_zone_has_no_cpu_reading()
    {
        QTemporaryDir root;
        write(root.filePath(QStringLiteral("intel-rapl:0:0/name")), "core\n");
        QVERIFY(!energy::findPackageZone(root.path()).has_value());
        QVERIFY(!energy::findPackageZone(root.filePath(QStringLiteral("missing"))).has_value());
    }

    void an_unreadable_counter_gives_no_reading()
    {
        QTemporaryDir root;
        QVERIFY(!energy::readNumber(root.filePath(QStringLiteral("energy_uj"))).has_value());
        write(root.filePath(QStringLiteral("garbage")), "not a number\n");
        QVERIFY(!energy::readNumber(root.filePath(QStringLiteral("garbage"))).has_value());
    }

    void the_history_keeps_only_the_newest_samples()
    {
        QList<double> history{1, 2, 3};
        energy::pushSample(history, 4, 3);
        QCOMPARE(history, (QList<double>{2, 3, 4}));
        energy::pushSample(history, 5, 5);
        QCOMPARE(history, (QList<double>{2, 3, 4, 5}));
    }

    // Regression: reopening the page used to count the whole closed interval at the old watts.
    void the_first_sample_after_the_page_reopens_counts_no_time()
    {
        energy::SampleClock clock;
        QCOMPARE(clock.next(1'000'000'000), 0.0);
        QCOMPARE(clock.next(3'000'000'000), 2.0);
        clock.restart();
        QCOMPARE(clock.next(500'000'000'000), 0.0);
        QCOMPARE(clock.next(501'500'000'000), 1.5);
    }
};

QTEST_GUILESS_MAIN(TestPower)
#include "test_power.moc"
