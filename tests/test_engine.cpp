#include <QtTest/QtTest>

#include <QApplication>
#include <QSettings>

#include <engine/engine.h>

class EngineTest : public QObject
{
    Q_OBJECT

private slots:
    void constructsCoreServices();
    void createsUnregisteredChannel();
    void rejectsUnknownChannelLookups();
    void addsAndDeletesChannel();
    void registersAndUnregistersOutputSink();
    void ignoresUnknownOutputWindowCommands();
};

void EngineTest::constructsCoreServices()
{
    MTGS::Engine engine;

    QVERIFY(engine.channelsModel() != nullptr);
    QVERIFY(engine.availableCamerasModel() != nullptr);
    QVERIFY(engine.cameraManager() != nullptr);
}

void EngineTest::createsUnregisteredChannel()
{
    MTGS::Engine engine;
    MTGS::Channel *channel = engine.createChannel();

    QVERIFY(channel != nullptr);
    QVERIFY(!channel->options().id.isEmpty());
    QCOMPARE(channel->options().windowGeometry, QRect(50, 50, 400, 400));
    QVERIFY(channel->camera() != nullptr);
    QVERIFY(channel->captureSession() != nullptr);
    QVERIFY(channel->metrics() != nullptr);
    QCOMPARE(channel->parent(), &engine);
    QVERIFY(!engine.channelExists(channel->options().id));

    channel->deleteLater();
}

void EngineTest::rejectsUnknownChannelLookups()
{
    MTGS::Engine engine;

    QVERIFY(!engine.channel(QStringLiteral("missing")));
    QVERIFY(!engine.channelAtIndex(-1));
    QVERIFY(!engine.channelAtIndex(0));
    QVERIFY(!engine.channelExists(QStringLiteral("missing")));
}

void EngineTest::addsAndDeletesChannel()
{
    MTGS::Engine engine;
    MTGS::Channel *channel = engine.createChannel();
    channel->options().name = QStringLiteral("Test channel");
    channel->options().windowName = QStringLiteral("Test output");
    channel->options().windowGeometry = QRect(10, 10, 320, 240);

    QSignalSpy added_spy(&engine, &MTGS::Engine::channelAdded);
    QSignalSpy about_to_delete_spy(&engine, &MTGS::Engine::channelAboutToBeDeleted);
    QSignalSpy deleted_spy(&engine, &MTGS::Engine::channelDeleted);
    const int initialRows = engine.channelsModel()->rowCount(QModelIndex{});
    const MTGS::ChannelOptions options = channel->options();

    engine.addChannel(channel, MTGS::Engine::Stopped);

    QVERIFY(engine.channelExists(options.id));
    QCOMPARE(engine.channel(options.id), channel);
    QCOMPARE(added_spy.count(), 1);
    QCOMPARE(engine.channelsModel()->rowCount(QModelIndex{}), initialRows + 1);
    QVERIFY(engine.cameraManager()->isCameraInUse(options.cameraDevice));

    engine.deleteChannel(options);

    QVERIFY(!engine.channelExists(options.id));
    QCOMPARE(about_to_delete_spy.count(), 1);
    QCOMPARE(deleted_spy.count(), 1);
    QCOMPARE(engine.channelsModel()->rowCount(QModelIndex{}), initialRows);
    QVERIFY(!engine.cameraManager()->isCameraInUse(options.cameraDevice));
}

void EngineTest::registersAndUnregistersOutputSink()
{
    MTGS::Engine engine;
    MTGS::Channel *channel = engine.createChannel();
    channel->options().name = QStringLiteral("Sink test channel");
    channel->options().windowName = QStringLiteral("Sink test output");
    const MTGS::ChannelOptions options = channel->options();
    engine.addChannel(channel, MTGS::Engine::Stopped);

    QVideoSink sink;
    engine.registerChannelOutSink(options.id, &sink);
    QCOMPARE(channel->outVideoSink(), &sink);

    MTGS::FramePtr frame = MTGS::FramePtr::create();
    frame->channelId = options.id;
    engine.receiveFrameNotification(frame);
    QVERIFY(!sink.videoFrame().isValid());

    engine.unRegisterChannelOutSink(options.id);
    QCOMPARE(channel->outVideoSink(), nullptr);

    engine.registerChannelOutSink(QStringLiteral("missing"), &sink);
    engine.unRegisterChannelOutSink(QStringLiteral("missing"));
    engine.deleteChannel(options);
}

void EngineTest::ignoresUnknownOutputWindowCommands()
{
    MTGS::Engine engine;

    engine.launchOutputWindow(QStringLiteral("missing"));
    engine.closeOutputWindow(QStringLiteral("missing"));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("MTGScannerTests"));
    QCoreApplication::setApplicationName(QStringLiteral("EngineTest"));
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, QDir::tempPath());
    return QTest::qExec(new EngineTest, argc, argv);
}

#include "test_engine.moc"