#pragma once

#include <cstddef>
#include <optional>

#include <QList>
#include <QtCore/QRect>
#include <QtCore/QPointF>
#include <QtCore/QString>
#include <QtGui/QImage>

#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

namespace MTGS {

struct KeyPoint {
    QPointF pt;
    float visibility;
    int id;
};

struct Prediction {
    QRect box;
    QList<KeyPoint> keypoints;
    float confidence;
    QString className;
    int classId;
    size_t trackerId = -1;
    
    std::optional<QList<QImage>> crops;
    std::optional<QList<Prediction>> subPredictions;
};

}