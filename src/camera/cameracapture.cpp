#include "cameracapture.h"
#include "core/frame.hpp"
#include <qloggingcategory.h>

namespace MTGS {

namespace tf = tbb::flow;

Q_STATIC_LOGGING_CATEGORY(capture_logger, "mtgs.capture")
CameraCapture::CameraCapture(const QString &channelId,
                            const QString &cameraId,
                            FramesPerSecond &fps,
                            FramesPerSecond &skippedFps,
                            QObject* parent)
    : QObject(parent)
    , m_channelId(channelId)
    , m_cameraId(cameraId)
    , m_fps(fps)
    , m_skippedFps(skippedFps)
{}

void CameraCapture::setGateway(tf::async_node<tf::continue_msg, FramePtr>::gateway_type *gateway)
{
    m_gateway = gateway;
}

void CameraCapture::onVideoFrameChanged(const QVideoFrame &frame)
{
    if (!frame.isValid())
        return;

    FramePtr f = FramePtr::create();
    f->frameOriginal = frame;
    f->sequenceId = m_frameSequenceCount++;
    f->channelId = m_channelId;
    f->cameraId = m_cameraId;
    f->timestamp = QTime::currentTime();

    if (!m_gateway || !m_gateway->try_put(f)) {
        m_frameSequenceCount--;
        m_skippedFps.update();
    }
    m_fps.update();
}

void CameraCapture::onErrorOccurred(const QString &channelName, QCamera::Error error, const QString &errorString)
{
    qCCritical(capture_logger) << "Channel" << channelName << error << errorString;
}

void CameraCapture::onPlayerErrorOccurred(const QString &channelName, QMediaPlayer::Error error, const QString &errorString)
{
    qCCritical(capture_logger) << "Channel" << channelName << error << errorString;
}

void CameraCapture::onActiveChanged(const QString &channelName, bool isActive)
{
    qCDebug(capture_logger) << "Channel" << channelName << "isActive:" << isActive;
}

}