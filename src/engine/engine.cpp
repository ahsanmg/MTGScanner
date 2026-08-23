#include "engine.h"

#include <algorithm>
#include <cstddef>
#include <tuple>

#include <QUrl>
#include <QFile>
#include <QList>
#include <QUuid>
#include <QObject>
#include <QThread>
#include <QScreen>
#include <QVariant>
#include <QSettings>
#include <QQmlEngine>
#include <QMetaObject>
#include <QSharedPointer>
#include <QCameraDevice>
#include <QMediaDevices>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QLoggingCategory>
#include <QtMultimedia/QVideoFrame>

#include <onnxruntime_cxx_api.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/core/persistence.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/highgui.hpp>

#include <core/frame.hpp>
#include <core/image.hpp>
#include <core/prediction.hpp>
#include <engine/channelraw.hpp>
#include <engine/carddetector.h>
#include <engine/cardprocessor.h>
#include <output/nameplatemodel.h>
#include <output/outputwindow.h>
#include <channel.hpp>

namespace MTGS {

using accessor = tbb::concurrent_hash_map<QString, QSharedPointer<ChannelRaw>>::accessor;
using const_accessor = tbb::concurrent_hash_map<QString, QSharedPointer<ChannelRaw>>::const_accessor;

// ------------------ Engine Implementation ------------------
Q_STATIC_LOGGING_CATEGORY(engine_logger, "mtgs.engine")
Engine::Engine(QObject *parent)
    : QObject(parent)
    , m_availableCamerasModel(new AvailableCamerasModel(this))
    , m_cameraMngr(new CameraManager(this))
    , m_metricsTimer(new QTimer(this))
    , m_channelsModel(new ChannelModel(this))
{
    CardDetectorConfig config;
    config.path = "assets/models/yolo11n-pose.onnx";
    m_cardDetector = QSharedPointer<CardDetector>::create(nullptr, config, Ort::SessionOptions{nullptr}, Ort::MemoryInfo{nullptr});
    m_cardDetector->printModelMetadata();
    m_cardDetector->setColors(QList<QColor>{
        QColor(255, 0, 0),       // Red
        QColor(0, 255, 0),       // Green
        QColor(0, 0, 255),       // Blue
        QColor(255, 255, 0),     // Yellow
        QColor(0, 255, 255),     // Cyan
        QColor(255, 0, 255),     // Magenta
        QColor(255, 165, 0),     // Orange
        QColor(255, 192, 203),   // Pink
        QColor(128, 0, 128),     // Purple
        QColor(128, 255, 0)      // Light Green / Mint
    });

    initializeGraph();

    // Connect CameraManager to the AvailableCamerasModel
    m_availableCamerasModel->setCameraManager(m_cameraMngr);

    // Connect channels model
    connect(this, &Engine::channelAdded, m_channelsModel, &ChannelModel::channelAdded);
    connect(this, &Engine::channelAboutToBeDeleted, m_channelsModel, &ChannelModel::channelDeleted);

#ifndef NDEBUG
    // Add a demo channel
    QUrl demo_file = QUrl("assets/videos/demo.mp4");
    if (demo_file.isValid() && QFile::exists(demo_file.path())) {
        DemoChannel *channel = new DemoChannel(this);
        channel->options().id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        channel->options().name = "Demo";
        channel->options().windowGeometry = QRect(50, 50, 400, 400);
        channel->setPlayer(new QMediaPlayer(channel));
        channel->player()->setSource(demo_file);
        channel->player()->setVideoSink(new QVideoSink(channel->player()));
        channel->setMetrics(new ChannelMetrics(channel));
        channel->setOutputWindowScreen(QGuiApplication::primaryScreen());

        addChannel(channel, Engine::Running);
    }
#endif

    // Load the saved channels
    loadFromSettings();

    m_metricsTimer->start(750);
}

Engine::~Engine() 
{
    // Save settings before stopping everything.
    saveToSettings();

    for (auto it = m_channels.begin(); it != m_channels.end(); ++it) {
        if (auto channel = qobject_cast<Channel*>(it.value()))
            channel->camera()->stop();
        else if (auto demo_channel = qobject_cast<DemoChannel*>(it.value()))
            demo_channel->player()->stop();
    }

    for (auto &[_, raw_channel] : m_rawChannels) {
        raw_channel->capture.thread->quit();
        raw_channel->capture.thread->wait();
    }

    m_graph.wait_for_all();
}

void Engine::initializeGraph()
{
    auto processor_body = [this](const FramePtr &frame, auto &ports) {
        // Check for expiration
        if (frame->timestamp.msecsTo(QTime::currentTime()) > 200) {
            frame->isExpired = true;

            std::get<0>(ports).try_put(frame);
            return;
        }

        float threshold = 0.4f;
        {
            accessor a;
            if (m_rawChannels.find(a, frame->channelId)
                && !a.empty()) {
                threshold = a->second->options.detectionThreshold;
            }
        }

        frame->frameImg = frame->frameImg.convertToFormat(QImage::Format_RGB888);
        frame->predictions = m_cardDetector->predict({ frame->frameImg }, threshold).at(0);

        std::get<0>(ports).try_put(frame);
        std::get<1>(ports).try_put(tf::continue_msg());
    };

    auto frame_distributor_body = [this](const FramePtr &frame, auto &ports) {
        accessor a;
        if (!m_rawChannels.find(a, frame->channelId)
            || a.empty()) {
            std::get<0>(ports).try_put(tf::continue_msg());
            qCDebug(engine_logger) << "Channel not found for frame" << frame->sequenceId << frame->channelId;
            return;
        }

        auto &channel = a->second;
        channel->postSequencer->try_put(frame);
        channel->preLimiter->decrementer().try_put(tf::continue_msg());
        std::get<0>(ports).try_put(tf::continue_msg());
    };

    auto ui_notifier_body = [this](const FramePtr &f) {
        accessor a;
        if (m_rawChannels.find(a, f->channelId) && !a.empty()) {
            auto &channel = a->second;

            // Handle the expired frame.
            if (f->isExpired) {
                qCWarning(engine_logger) << QString("Frame %1 expired, channel %2, current time difference %3ms." )
                    .arg(f->sequenceId)
                    .arg(channel->options.name)
                    .arg(f->timestamp.msecsTo(QTime::currentTime()));

                channel->skippedFps.update();
                return;
            }

            if (channel->cardProcessor)
                channel->cardProcessor->process(f);

            channel->fps.update();
            channel->visibleCards = f->predictions.size();  // card processor rearranges cards and its parts.
        }
        a.release();
        m_cardDetector->draw(f->frameImg, f->predictions, true, true, false);
        f->frameOriginal = QVideoFrame(f->frameImg);
        QMetaObject::invokeMethod(this, "receiveFrameNotification", Qt::QueuedConnection, Q_ARG(FramePtr, f));
    };

    // The critical processor node.
    m_processor = QSharedPointer<tf::multifunction_node<FramePtr, std::tuple<FramePtr, tf::continue_msg>>>::create(m_graph, tf::unlimited, processor_body);
    m_frameDistributor = QSharedPointer<tf::multifunction_node<FramePtr, std::tuple<tf::continue_msg>>>::create(m_graph, tf::unlimited, frame_distributor_body);
    m_uiNotifier = QSharedPointer<tf::function_node<FramePtr>>::create(m_graph, tf::serial, ui_notifier_body);

    tf::make_edge(tf::output_port<0>(*m_processor), *m_frameDistributor);
}

ChannelModel* Engine::channelsModel() const
{
    return m_channelsModel;
}

AvailableCamerasModel* Engine::availableCamerasModel() const
{
    return m_availableCamerasModel;
}

Channel *Engine::createChannel()
{
    Channel *channel = new Channel(this);
    channel->options().id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    channel->options().windowGeometry = QRect(50, 50, 400, 400);
    channel->setCamera(new QCamera(channel));
    channel->setCaptureSession(new QMediaCaptureSession(channel));
    channel->captureSession()->setCamera(channel->camera());
    channel->setMetrics(new ChannelMetrics(channel));
    channel->setOutputWindowScreen(QGuiApplication::primaryScreen());

    QQmlEngine::setObjectOwnership(channel, QQmlEngine::CppOwnership);
    return channel;
}

AbstractChannel *Engine::channel(const QString &channelId)
{
    if (!m_channels.contains(channelId)) {
        qCCritical(engine_logger) << QString("Requsted channel for a non existent id %1.").arg(channelId);
        return nullptr;
    }

    AbstractChannel *channel = m_channels.value(channelId);
    QQmlEngine::setObjectOwnership(channel, QQmlEngine::CppOwnership);
    return channel;
}

AbstractChannel *Engine::channelAtIndex(int index)
{
    if (index < 0 || index >= m_channelIdIndexMap.size()) {
        qCCritical(engine_logger) << QString("Requsted channel for a non existent index %1.").arg(index);
        return nullptr;
    }

    return channel(m_channelIdIndexMap.at(index));
}

void Engine::destroyChannel(Channel *channel)
{
    if (!channel)
        return;

    channel->deleteLater();
}

bool Engine::channelExists(const QString &id)
{
    return m_channels.contains(id);
}

CameraManager *Engine::cameraManager()
{
    return m_cameraMngr;
}

void Engine::saveToSettings()
{
    QSettings settings;
    settings.beginWriteArray("channels");

    size_t i = 0;
    for (auto it = m_channels.cbegin(); it != m_channels.cend(); ++it) {
        if (it.value()->channelType() == "DemoChannel")
            continue;

        const auto& options = it.value()->options();
        settings.setArrayIndex(i++);
        settings.setValue("id", options.id);
        settings.setValue("name", options.name);
        settings.setValue("maxInFlight", options.maxInFlight);
        settings.setValue("detThresh", options.detectionThreshold);
        settings.setValue("cameraDeviceId", options.cameraDevice.id());
        settings.setValue("cameraDeviceDesc", options.cameraDevice.description());
        settings.setValue("winName", options.windowName);
        settings.setValue("winGeometry", options.windowGeometry);
        settings.setValue("screenSerialNo", it.value()->outputWindowScreen()->serialNumber());
        settings.setValue("screenName", it.value()->outputWindowScreen()->name());
        settings.setValue("status", it.value()->metrics()->status());
    }

    settings.endArray();
}

void Engine::loadFromSettings()
{
    QList<QScreen*> screens = QGuiApplication::screens();

    QSettings settings;
    auto size = settings.beginReadArray("channels");
    for(decltype(size) i = 0; i < size; ++i) {
        settings.setArrayIndex(i);

        ChannelOptions options;
        options.id = settings.value("id").toString();
        options.name = settings.value("name").toString();
        options.maxInFlight = settings.value("maxInFlight").toInt();
        options.detectionThreshold = settings.value("detThresh").toFloat();
        options.windowName = settings.value("winName").toString();
        options.windowGeometry = settings.value("winGeometry").toRect();
        int channelStatus = settings.value("status").toInt();

        // Verify CameraDevice
        const QString camera_id = settings.value("cameraDeviceId").toString();
        const QString camera_desc = settings.value("cameraDeviceDesc").toString();
        auto camera_opt = m_cameraMngr->findBy(camera_id, camera_desc);

        // Verify Screen
        QString screen_serial_no = settings.value("screenSerialNo").toString();
        QString screen_name = settings.value("screenName").toString();
        const auto screen_it = std::find_if(screens.cbegin(), screens.cend(),
            [&] (QScreen *s) {
                return s->serialNumber() == screen_serial_no || s->name() == screen_name;
            }
        );

        // Add the channel
        Channel *channel = createChannel();

        if (camera_opt) {
            options.cameraDevice = camera_opt.value();
            channel->camera()->setCameraDevice(options.cameraDevice);
        } else {
            channelStatus = ChannelStatus::Stopped;
        }

        if (screen_it != screens.end())
            channel->setOutputWindowScreen(*screen_it);

        channel->setOptions(options);
        addChannel(channel, channelStatus);
    }

    settings.endArray();
}

void Engine::receiveFrameNotification(const FramePtr& frame)
{
    AbstractChannel *channel = m_channels.value(frame->channelId, nullptr);
    if (!channel) {
        qCDebug(engine_logger) << QString("Stranded frame %1 from channel %2").arg(frame->sequenceId).arg(frame->channelId);
        return;
    }

    auto videoSink = channel->outVideoSink();
    if (videoSink)
        videoSink->setVideoFrame(frame->frameOriginal);

    OutputWindow *window = m_outputWindows.value(frame->channelId, nullptr);
    if (!window)
        return;

    // Send crops to each channel's output window
    for(const auto &p : frame->predictions) {
        if (!p.crops) continue;

        for (const auto &crop : p.crops.value())
            if (auto model = window->model())
                model->addNameplate(p.trackerId, crop);
    }
}

void Engine::addChannel(AbstractChannel *channel, int status)
{
    if (!channel)
        return;

    // Debug channel destruction
#ifndef NDEBUG
    connect(channel, &Channel::destroyed, [=] () {
        qCCritical(engine_logger) << QString("Channel got destroyed, named %1, id %2.").arg(channel->options().name).arg(channel->options().id);
    });
#endif

    Channel *live_channel = qobject_cast<Channel*>(channel);
    DemoChannel *demo_channel = qobject_cast<DemoChannel*>(channel);
    if (live_channel) {
        // Mark the camera first. This function expects the QCamera
        // to have already been activated before.
        m_cameraMngr->setCameraInUse(channel->options().cameraDevice, true);

        live_channel->camera()->stop();
        live_channel->captureSession()->setVideoOutput(nullptr);
        live_channel->captureSession()->setVideoSink(new QVideoSink(live_channel->captureSession()));
    }

    auto channel_raw = QSharedPointer<ChannelRaw>::create();

    // Bind metrics
    channel->metrics()->setFps(&channel_raw->fps);
    channel->metrics()->setCaptureFps(&channel_raw->captureFps);
    channel->metrics()->setSkippedFps(&channel_raw->skippedFps);
    channel->metrics()->setStatus(&channel_raw->status);
    channel->metrics()->setVisibleCards(&channel_raw->visibleCards);
    connect(m_metricsTimer, &QTimer::timeout, channel->metrics(), &ChannelMetrics::fireMetricsUpdate);

    // Per-channel nodes and edges
    auto async_src_body = [] (const tf::continue_msg &, tf::async_node<tf::continue_msg, FramePtr>::gateway_type& gateway) {};
    auto sequencer_body = [] (const FramePtr &f) { return f->sequenceId; };
    channel_raw->asyncSrc = QSharedPointer<tf::async_node<tf::continue_msg, FramePtr>>::create(m_graph, tf::unlimited, async_src_body);
    channel_raw->preLimiter = QSharedPointer<tf::limiter_node<FramePtr>>::create(m_graph, channel->options().maxInFlight);
    channel_raw->postSequencer = QSharedPointer<tf::sequencer_node<FramePtr>>::create(m_graph, sequencer_body);

    tf::make_edge(*channel_raw->asyncSrc, *channel_raw->preLimiter);
    tf::make_edge(*channel_raw->preLimiter, *m_processor);
    tf::make_edge(*channel_raw->postSequencer, *m_uiNotifier);

    // Camera Capture setup
    auto *thread = new QThread();
    auto *capture = new CameraCapture(channel->options().id, channel->options().cameraDevice.id(), channel_raw->captureFps, channel_raw->skippedFps);
    capture->setGateway(&channel_raw->asyncSrc->gateway());
    connect(thread, &QThread::finished, capture, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    if (live_channel) {
        connect(live_channel->captureSession()->videoSink(), &QVideoSink::videoFrameChanged, capture, &CameraCapture::onVideoFrameChanged);
        connect(live_channel->camera(), &QCamera::errorOccurred,
            [this, channel = live_channel, capture = capture] (QCamera::Error error, const QString &errorStr) {
                accessor a;
                if (m_rawChannels.find(a, channel->options().id) && !a.empty()) {
                    auto &raw_channel = a->second;
                    raw_channel->status.storeRelaxed(Engine::Errored);
                }
    
                QMetaObject::invokeMethod(capture,
                    "onErrorOccurred",
                    Q_ARG(QString, channel->options().name),
                    Q_ARG(QCamera::Error, error),
                    Q_ARG(QString, errorStr));
            }
        );
        connect(live_channel->camera(), &QCamera::activeChanged,
            [this, channel = live_channel, capture = capture] (bool active) {
                accessor a;
                if (m_rawChannels.find(a, channel->options().id) && !a.empty()) {
                    auto &raw_channel = a->second;
                    if (raw_channel->status.loadRelaxed() == Engine::Errored) {
                        // This is an error scenario, not a common start/stop one.
                        // We let that be.
                        return;
                    }
        
                    raw_channel->status.storeRelease(active ? Engine::Running : Engine::Stopped);
                }
    
                QMetaObject::invokeMethod(capture,
                    "onActiveChanged",
                    Q_ARG(QString, channel->options().name),
                    Q_ARG(bool, active));
            }
        );
    } else if (demo_channel) {
        connect(demo_channel->player()->videoSink(), &QVideoSink::videoFrameChanged, capture, &CameraCapture::onVideoFrameChanged);
        connect(demo_channel->player(), &QMediaPlayer::errorOccurred,
            [this, channel = demo_channel, capture = capture] (QMediaPlayer::Error error, const QString &errorStr) {
                accessor a;
                if (m_rawChannels.find(a, channel->options().id) && !a.empty())
                    a->second->status.storeRelaxed(Engine::Errored);

                QMetaObject::invokeMethod(capture,
                    "onPlayerErrorOccurred",
                    Q_ARG(QString, channel->options().name),
                    Q_ARG(QMediaPlayer::Error, error),
                    Q_ARG(QString, errorStr));
            }
        );
        connect(demo_channel->player(), &QMediaPlayer::playbackStateChanged,
            [this, channel = demo_channel, capture = capture] (QMediaPlayer::PlaybackState status) {
                accessor a;
                if (m_rawChannels.find(a, channel->options().id) && !a.empty()) {
                    auto &raw = a->second;
                    if (raw->status.loadRelaxed() == Engine::Errored) {
                        // This is an error scenario, not a common start/stop one.
                        // We let that be.
                        return;
                    }

                    bool active = status == QMediaPlayer::PlayingState;
                    raw->status.storeRelease(active ? Engine::Running : Engine::Stopped);
                }

                QMetaObject::invokeMethod(capture,
                    "onActiveChanged",
                    Q_ARG(QString, channel->options().name),
                    Q_ARG(bool, status == QMediaPlayer::PlayingState));
            }
        );
    }
    
    capture->moveToThread(thread);
    channel_raw->capture = {thread, capture};
    channel_raw->cardProcessor = QSharedPointer<CardProcessor>::create(30, 60);
    channel_raw->options = channel->options();
    channel_raw->status.storeRelaxed(status);
    channel_raw->capture.thread->start();
    channel_raw->skippedFps.start();
    channel_raw->fps.start();

    if (!m_rawChannels.emplace(channel->options().id, channel_raw)) {
        qCCritical(engine_logger) << "Failed to insert a new channel after connecting it, named" << channel->options().name;
        disconnectChannel(channel, channel_raw);
        channel->deleteLater();
        return;
    }

    auto model = new NameplateModel(20);
    OutputWindow *window = new OutputWindow(true,
                                            true,
                                            model,
                                            nullptr);
    window->setWindowTitle(channel->options().windowName);
    window->setGeometry(channel->options().windowGeometry);
    window->setScreen(channel->outputWindowScreen());
    window->setWindowIcon(indexedWindowIcon(":/MTGScanner/icons/app_titled.svg", m_outputWindows.size() + 1));
    model->setParent(window);
    m_outputWindows.emplace(channel->options().id, window);
    if (m_mainQmlWindow)
        window->open(m_mainQmlWindow);

    if (live_channel && channel->metrics()->status() != Engine::Stopped) {
        // Then it's probably Unknown
        live_channel->camera()->start();
    }

    if (demo_channel)
        demo_channel->player()->play();

    m_channels.emplace(channel->options().id, channel);
    m_channelIdIndexMap.append(channel->options().id);
    emit channelAdded(channel->options());
}

void Engine::deleteChannel(const ChannelOptions &options)
{
    auto *channel = m_channels.value(options.id, nullptr);
    if (!channel || channel->channelType() != "Channel") {
        qCCritical(engine_logger) << QString("Trying to delete a non-existent channel using id %1.").arg(options.id);
        return;
    }

    // In case someone wants to opt-out
    emit channelAboutToBeDeleted(options);

    QSharedPointer<ChannelRaw> raw_channel = nullptr;
    accessor a;
    if (!m_rawChannels.find(a, channel->options().id) || a.empty()) {
        qCCritical(engine_logger) << "Couldn't find a raw channel with id" << channel->options().id;   
        raw_channel = a->second;
    }
    disconnectChannel(channel, raw_channel);

    // Delete
    m_channels.take(options.id)->deleteLater();
    m_channelIdIndexMap.removeOne(options.id);
    m_rawChannels.erase(a);

    emit channelDeleted(options);
}

void Engine::disconnectChannel(AbstractChannel *channel, QSharedPointer<ChannelRaw> raw_channel)
{
    if (auto _channel = qobject_cast<Channel*>(channel)) {
        _channel->camera()->disconnect();   // This should help avoid a deadlock in the errorOccurred and activeChanged
        _channel->camera()->stop();
    } else if (auto demo_channel = qobject_cast<DemoChannel*>(channel)) {
        demo_channel->player()->disconnect();
        demo_channel->player()->stop();
    }

    // Metrics
    disconnect(m_metricsTimer, &QTimer::timeout, channel->metrics(), &ChannelMetrics::fireMetricsUpdate);

    channel->metrics()->setFps(nullptr);
    channel->metrics()->setCaptureFps(nullptr);
    channel->metrics()->setSkippedFps(nullptr);
    channel->metrics()->setStatus(nullptr);
    channel->metrics()->setVisibleCards(nullptr);

    // Raw Channel
    if (raw_channel) {
        raw_channel->capture.worker->setGateway(nullptr);
        raw_channel->capture.thread->quit();
        if (!raw_channel->capture.thread->wait(1000)) {
            qCWarning(engine_logger) << "Capture Thread failed to exit cleanly. Forcing termination for" << channel->options().id;
            raw_channel->capture.thread->terminate();
            raw_channel->capture.thread->wait();
        }

        tf::remove_edge(*(raw_channel->preLimiter), *m_processor);
        tf::remove_edge(*(raw_channel->asyncSrc),   *(raw_channel->preLimiter));
        tf::remove_edge(*(raw_channel->postSequencer), *m_uiNotifier);
    }

    if (auto _channel = qobject_cast<Channel*>(channel)) {
        // Explicitly delete the camera, to release the handle.
        if (auto *camera = _channel->camera()) {
            camera->disconnect(); // i.e. CameraCapture
            camera->deleteLater();
        }
        _channel->setCamera(nullptr);
        // Then set the camera-in-use to false
        auto device = _channel->options().cameraDevice;
        if (m_cameraMngr->isCameraInUse(device))
            m_cameraMngr->setCameraInUse(device, false);
        else
            qCCritical(engine_logger) << QString("Unexpected, Camera %1 was not in use during destruction.").arg(device.id());
    }
}

void Engine::startChannel(const QString &channelId)
{
    if (!m_channels.contains(channelId)) {
        qCCritical(engine_logger) << QString("Trying to start a non existent channel using id %1.").arg(channelId);
        return;
    }

    auto channel = m_channels.value(channelId);
    if (channel->metrics()->status() != ChannelStatus::Running) {
        channel->start();
        // EngineWoker will change the status
    }
}

void Engine::stopChannel(const QString &channelId)
{
    if (!m_channels.contains(channelId)) {
        qCCritical(engine_logger) << QString("Trying to stop a non existent channel using id %1.").arg(channelId);
        return;
    }

    auto channel = m_channels.value(channelId);
    if (channel->metrics()->status() != ChannelStatus::Stopped) {
        channel->stop();
        // EngineWoker will change the status
    }
}

void Engine::registerChannelOutSink(const QString &channelId, QVideoSink *videoSink)
{
    if (!m_channels.contains(channelId)) {
        qCDebug(engine_logger) << QString("Trying to register videosink for unknown channel %1").arg(channelId);
        return;
    }

    m_channels.value(channelId)->setOutVideoSink(videoSink);
}

void Engine::unRegisterChannelOutSink(const QString &channelId)
{
    if (!m_channels.contains(channelId))
        return;

    m_channels.value(channelId)->setOutVideoSink(nullptr);
}

void Engine::initializeOutputWindows(QQuickWindow *mainWindow)
{
    for (auto [_, window] : m_outputWindows.asKeyValueRange()) {
        window->open(mainWindow);
    }

    m_mainQmlWindow = mainWindow;
}

void Engine::launchOutputWindow(const QString &channelId)
{
    m_outputWindows.value(channelId)->open(m_mainQmlWindow);
}
void Engine::closeOutputWindow(const QString &channelId)
{
    OutputWindow *window = m_outputWindows.value(channelId);
    window->windowHandle()->setTransientParent(nullptr);
    window->hide();
}

} // namespace MTGS