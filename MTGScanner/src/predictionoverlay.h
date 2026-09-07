#pragma once

#include <atomic>

#include <QtCore/QSize>
#include <QtCore/QList>
#include <QtQuick/QSGNode>
#include <QtQuick/QQuickItem>
#include <QtQuick/QSGGeometryNode>
#include <QtGui/QPaintDevice>
#include <QtQmlIntegration/QtQmlIntegration>

#include "prediction.hpp"

namespace MTGS {

// This class is strictly supposed to be called from the main thread
class PredictionOverlay : public QQuickItem {
    Q_OBJECT
    QML_ELEMENT
public:
    explicit PredictionOverlay(QQuickItem *parent = nullptr);
    void updatePredictions(QList<Prediction> &&predictions, QSize sourceSize);

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private:
    QSize m_sourceSize;
    QRectF m_contentRect;
    QList<Prediction> m_predictions;
    std::atomic_bool m_dirty = false;
};

}
