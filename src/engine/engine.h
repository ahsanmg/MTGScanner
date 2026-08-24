#pragma once

#include <tuple>

#include <tbb_patched.hpp>

#include <QList>
#include <QHash>
#include <QTimer>
#include <QObject>
#include <QString>
#include <QVideoSink>
#include <QSettings>
#include <QQuickWindow>
#include <QSharedPointer>
#include <QCameraDevice>

#include <camera/availablecamerasmodel.h>
#include <camera/cameracapture.h>
#include <camera/cameramanager.h>
#include <engine/carddetector.h>
#include <engine/channelmodel.h>
#include <engine/channelraw.hpp>
#include <output/outputwindow.h>
#include <channel.hpp>
#include <predictionoverlay.h>

namespace MTGS {

// Engine
class Engine : public QObject {
    Q_OBJECT
    Q_PROPERTY(MTGS::ChannelModel* channelsModel READ channelsModel CONSTANT)
    Q_PROPERTY(MTGS::AvailableCamerasModel* availableCamerasModel READ availableCamerasModel CONSTANT)
public:
    enum ChannelStatus {
        Unknown,
        Initializing,
        Starting,
        Running,
        Stopping,
        Stopped,
        Errored
    };
    Q_ENUM(ChannelStatus)

    Engine(QObject *parent = nullptr);
    ~Engine();
    ChannelModel* channelsModel() const;
    AvailableCamerasModel* availableCamerasModel() const;
    Q_INVOKABLE Channel *createChannel();
    Q_INVOKABLE AbstractChannel *channel(const QString &channelId);
    Q_INVOKABLE AbstractChannel *channelAtIndex(int index);
    Q_INVOKABLE void destroyChannel(Channel *channel);
    Q_INVOKABLE bool channelExists(const QString &id);
    CameraManager *cameraManager();

public slots:
    void saveToSettings();
    void loadFromSettings();
    void receiveFrameNotification(const MTGS::FramePtr& frame);
    void addChannel(AbstractChannel *channel, int status = ChannelStatus::Unknown);
    void deleteChannel(const MTGS::ChannelOptions &options);
    void startChannel(const QString &channelId);
    void stopChannel(const QString &channelId);
    void registerPredictionOverlay(PredictionOverlay *overlay);
    void registerChannelOutSink(const QString &channelId, QVideoSink *videoSink);
    void unRegisterChannelOutSink(const QString &channelId);
    void initializeOutputWindows(QQuickWindow *mainWindow);
    void launchOutputWindow(const QString &channelId);
    void closeOutputWindow(const QString &channelId);

signals:
    void channelAdded(const MTGS::ChannelOptions &options);
    void channelAboutToBeDeleted(const MTGS::ChannelOptions &options);
    void channelDeleted(const MTGS::ChannelOptions &options);
    void channelStarted(bool started = true);
    void channelStopped(bool stopped = true);

private slots:
    void initializeGraph();

private:
    void disconnectChannel(AbstractChannel *channel, QSharedPointer<ChannelRaw> raw_channel = nullptr);

private:
    tf::graph m_graph;
    QSharedPointer<tf::multifunction_node<FramePtr, std::tuple<FramePtr, tf::continue_msg>>> m_processor;
    QSharedPointer<tf::multifunction_node<FramePtr, std::tuple<tf::continue_msg>>> m_frameDistributor;
    QSharedPointer<tf::function_node<FramePtr>> m_uiNotifier;

    QSharedPointer<CardDetector> m_cardDetector;

    tbb::concurrent_hash_map<QString, QSharedPointer<ChannelRaw>> m_rawChannels;
    QHash<QString, AbstractChannel*> m_channels;
    QHash<QString, OutputWindow*> m_outputWindows;
    QList<QString> m_channelIdIndexMap;

    PredictionOverlay *m_predictionOverlay = nullptr;
    AvailableCamerasModel *m_availableCamerasModel = nullptr;
    CameraManager *m_cameraMngr = nullptr;
    ChannelModel *m_channelsModel = nullptr;
    
    QTimer *m_metricsTimer = nullptr;

    QQuickWindow *m_mainQmlWindow = nullptr;
};

} // namespace MTGS