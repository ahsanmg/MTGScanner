#include <QtTest/QtTest>

#include <channelmetrics.h>

class ChannelMetricsTest : public QObject
{
    Q_OBJECT

private slots:
    void returnsDefaultsWithoutSources();
    void exposesStatusAndVisibleCards();
    void exposesStatusColors();
    void exposesFramesPerSecondSources();
};

void ChannelMetricsTest::returnsDefaultsWithoutSources()
{
    MTGS::ChannelMetrics metrics;

    QCOMPARE(metrics.status(), 0);
    QCOMPARE(metrics.statusColor(), QColor(QStringLiteral("red")));
    QCOMPARE(metrics.fps(), -1);
    QCOMPARE(metrics.captureFps(), -1);
    QCOMPARE(metrics.skippedFps(), -1);
    QCOMPARE(metrics.visibleCards(), -1);
}

void ChannelMetricsTest::exposesStatusAndVisibleCards()
{
    MTGS::ChannelMetrics metrics;
    QAtomicInt status{3};
    QAtomicInt visible_cards{7};

    metrics.setStatus(&status);
    metrics.setVisibleCards(&visible_cards);

    QCOMPARE(metrics.status(), 3);
    QCOMPARE(metrics.visibleCards(), 7);

    status.storeRelease(5);
    visible_cards.storeRelease(2);
    QCOMPARE(metrics.status(), 5);
    QCOMPARE(metrics.visibleCards(), 2);
}

void ChannelMetricsTest::exposesStatusColors()
{
    MTGS::ChannelMetrics metrics;
    QAtomicInt status{0};
    metrics.setStatus(&status);

    const QList<QColor> expected {
        QColor(QStringLiteral("red")),
        QColor(QStringLiteral("#95a5a6")),
        QColor(QStringLiteral("#f1c40f")),
        QColor(QStringLiteral("#2ecc71")),
        QColor(QStringLiteral("#7f8c8d")),
        QColor(QStringLiteral("#e74c3c")),
        QColor(QStringLiteral("#34495e"))
    };

    for (int index = 0; index < expected.size(); ++index) {
        status.storeRelease(index);
        QCOMPARE(metrics.statusColor(), expected.at(index));
    }

    status.storeRelease(7);
    QCOMPARE(metrics.statusColor(), expected.at(0));
}

void ChannelMetricsTest::exposesFramesPerSecondSources()
{
    MTGS::ChannelMetrics metrics;
    FramesPerSecond fps;
    FramesPerSecond capture_fps;
    FramesPerSecond skipped_fps;

    metrics.setFps(&fps);
    metrics.setCaptureFps(&capture_fps);
    metrics.setSkippedFps(&skipped_fps);

    QCOMPARE(metrics.fps(), 0);
    QCOMPARE(metrics.captureFps(), 0);
    QCOMPARE(metrics.skippedFps(), 0);

    fps.update();
    capture_fps.update();
    skipped_fps.update();
    QVERIFY(metrics.fps() > 0);
    QVERIFY(metrics.captureFps() > 0);
    QVERIFY(metrics.skippedFps() > 0);
}

QTEST_MAIN(ChannelMetricsTest)
#include "test_channelmetrics.moc"