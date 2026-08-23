#pragma once

#include <cstddef>

#include <QTime>
#include <QString>
#include <QObject>
#include <QVideoFrame>

#include <opencv2/core.hpp>
#include <opencv2/core/mat.hpp>

#include <prediction.hpp>

namespace MTGS {
    
struct Frame {
    bool isExpired = false;
    size_t sequenceId;
    QString channelId;
    QString cameraId;
    QTime timestamp;

    QImage frameImg;
    QVideoFrame frameOriginal;
    QList<Prediction> predictions;
};

using FramePtr = QSharedPointer<Frame>;

}