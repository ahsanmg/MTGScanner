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
    Q_PROPERTY(QRectF contentRect READ contentRect WRITE setContentRect NOTIFY contentRectChanged FINAL)
public:
    explicit PredictionOverlay(QQuickItem *parent = nullptr);
    void updatePredictions(QList<Prediction> &&predictions);
    QRectF contentRect() const;

public slots:
    void setContentRect(const QRectF &rect);

signals:
    void contentRectChanged();

protected:
    QSGNode *updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) override;

private:
    QRectF m_contentRect;
    QList<Prediction> m_predictions;
    std::atomic_bool m_dirty = false;
};

}
