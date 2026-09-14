#include <QtTest/QtTest>

#include <framespersecond.hpp>

class FramesPerSecondTest : public QObject
{
    Q_OBJECT

private slots:
    void startsWithoutEvents();
    void countsEventsAndCalculatesRate();
    void startClearsPreviousEvents();
    void retainsOnlyConfiguredRecentEvents();
};

void FramesPerSecondTest::startsWithoutEvents()
{
    FramesPerSecond counter;

    QCOMPARE(counter.eventCount(), 0);
    QCOMPARE(counter.fps(), 0.0);
}

void FramesPerSecondTest::countsEventsAndCalculatesRate()
{
    FramesPerSecond counter;
    counter.update();
    counter.update();

    QCOMPARE(counter.eventCount(), 2);
    QVERIFY(counter.fps() > 0.0);
}

void FramesPerSecondTest::startClearsPreviousEvents()
{
    FramesPerSecond counter;
    counter.update();
    counter.update();
    QCOMPARE(counter.eventCount(), 2);

    counter.start();

    QCOMPARE(counter.eventCount(), 0);
    QCOMPARE(counter.fps(), 0.0);
}

void FramesPerSecondTest::retainsOnlyConfiguredRecentEvents()
{
    constexpr int maxEvents = 2;
    FramesPerSecond counter(maxEvents);
    for (int index = 0; index < maxEvents + 101; ++index)
        counter.update();

    QCOMPARE(counter.eventCount(), maxEvents);
}

QTEST_MAIN(FramesPerSecondTest)
#include "test_framespersecond.moc"