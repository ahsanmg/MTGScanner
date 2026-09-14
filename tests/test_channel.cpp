#include <QtTest/QtTest>

#include <QSignalSpy>

#include <channel.hpp>

class TestAbstractChannel : public MTGS::AbstractChannel
{
public:
    explicit TestAbstractChannel(QObject *parent = nullptr)
        : AbstractChannel(QStringLiteral("TestChannel"), parent)
    {}

    void start() override {}
    void stop() override {}
};

class ChannelTest : public QObject
{
    Q_OBJECT

private slots:
    void optionsStartInvalidAndCompare();
    void exposesAndUpdatesAbstractChannelProperties();
    void updatesConcreteChannelProperties();
};

void ChannelTest::optionsStartInvalidAndCompare()
{
    MTGS::ChannelOptions first;
    QVERIFY(!first.isValid());

    first.id = QStringLiteral("channel-1");
    first.name = QStringLiteral("Camera");
    first.windowName = QStringLiteral("Output");
    QVERIFY(!first.isValid());

    auto second = first;
    QCOMPARE(first, second);
    second.maxInFlight++;
    QVERIFY(first != second);
}

void ChannelTest::exposesAndUpdatesAbstractChannelProperties()
{
    TestAbstractChannel channel;
    QVERIFY(channel.channelType() == QStringLiteral("TestChannel"));

    MTGS::ChannelOptions options;
    options.id = QStringLiteral("channel-1");
    options.name = QStringLiteral("Camera");
    QSignalSpy options_spy(&channel, &MTGS::AbstractChannel::optionsChanged);

    channel.setOptions(options);
    QCOMPARE(channel.constOptions(), options);
    QCOMPARE(options_spy.count(), 1);

    channel.setOptions(options);
    QCOMPARE(options_spy.count(), 1);

    MTGS::ChannelMetrics metrics;
    QSignalSpy metrics_spy(&channel, &MTGS::AbstractChannel::metricsChanged);
    channel.setMetrics(&metrics);
    QCOMPARE(channel.metrics(), &metrics);
    QCOMPARE(metrics_spy.count(), 1);

    QSignalSpy sink_spy(&channel, &MTGS::AbstractChannel::outVideoSinkChanged);
    QVideoSink sink;
    channel.setOutVideoSink(&sink);
    QCOMPARE(channel.outVideoSink(), &sink);
    QCOMPARE(sink_spy.count(), 1);
}

void ChannelTest::updatesConcreteChannelProperties()
{
    MTGS::Channel channel;
    QCamera camera;
    QMediaCaptureSession capture_session;
    QSignalSpy camera_spy(&channel, &MTGS::Channel::cameraChanged);
    QSignalSpy session_spy(&channel, &MTGS::Channel::captureSessionChanged);

    channel.setCamera(&camera);
    channel.setCaptureSession(&capture_session);

    QCOMPARE(channel.camera(), &camera);
    QCOMPARE(channel.captureSession(), &capture_session);
    QCOMPARE(camera_spy.count(), 1);
    QCOMPARE(session_spy.count(), 1);
}

QTEST_MAIN(ChannelTest)
#include "test_channel.moc"