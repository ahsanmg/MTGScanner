#include "predictionoverlay.h"

#include <QtQuick/QSGFlatColorMaterial>

namespace MTGS {

PredictionOverlay::PredictionOverlay(QQuickItem *parent)
    : QQuickItem(parent)
{
    setFlag(ItemHasContents, true); // Enables scene graph rendering
}

void PredictionOverlay::updatePredictions(QList<Prediction> &&predictions, QSize sourceSize) {
    m_predictions = std::move(predictions);
    m_sourceSize = sourceSize;
    m_dirty = true;
    update();
}

QSGNode *PredictionOverlay::updatePaintNode(QSGNode *oldNode, UpdatePaintNodeData *) {
    auto *node = static_cast<QSGGeometryNode *>(oldNode);

    if (!node) {
        node = new QSGGeometryNode();

        auto *geometry = new QSGGeometry(QSGGeometry::defaultAttributes_Point2D(), 0);
        geometry->setDrawingMode(QSGGeometry::DrawLines);
        geometry->setLineWidth(2.0f);
        node->setGeometry(geometry);
        node->setFlag(QSGNode::OwnsGeometry);

        auto *material = new QSGFlatColorMaterial();
        material->setColor(QColor("lime"));
        node->setMaterial(material);
        node->setFlag(QSGNode::OwnsMaterial);
    }

    if (!m_dirty)
        return node;
    m_dirty = false;

    // Handle empty frame
    if (m_predictions.isEmpty()
        || m_sourceSize.width() <= 0
        || m_sourceSize.height() <= 0) {
        node->geometry()->allocate(0);
        node->markDirty(QSGNode::DirtyGeometry);
        return node;
    }

    const float scale_x = static_cast<float>(width()) / static_cast<float>(m_sourceSize.width());
    const float scale_y = static_cast<float>(height()) / static_cast<float>(m_sourceSize.height());

    constexpr int k_box_vertices = 8;
    constexpr int k_keypoint_vertices = 4;
    int vertex_count = 0;
    for (const auto &pred : m_predictions)
        vertex_count += k_box_vertices + (pred.keypoints.size() * k_keypoint_vertices);

    auto *geometry = node->geometry();
    geometry->allocate(vertex_count);
    
    if (vertex_count == 0) {
        node->markDirty(QSGNode::DirtyGeometry);
        return node;
    }
    
    int index = 0;
    auto *vertices = geometry->vertexDataAsPoint2D();
    for (const auto &pred : m_predictions) {
        const float x1 = static_cast<float>(pred.box.x()) * scale_x;
        const float y1 = static_cast<float>(pred.box.y()) * scale_y;
        const float x2 = static_cast<float>(pred.box.x() + pred.box.width()) * scale_x;
        const float y2 = static_cast<float>(pred.box.y() + pred.box.height()) * scale_y;

        // Bounding Box Vertices
        vertices[index++].set(x1, y1); vertices[index++].set(x2, y1); // Top line
        vertices[index++].set(x2, y1); vertices[index++].set(x2, y2); // Right line
        vertices[index++].set(x2, y2); vertices[index++].set(x1, y2); // Bottom line
        vertices[index++].set(x1, y2); vertices[index++].set(x1, y1); // Left line

        constexpr float k_crosshair_radius = 4.0f;
        for (const auto &kp : pred.keypoints) {
            if (index + k_keypoint_vertices > vertex_count)
                break;

            const float kx = static_cast<float>(kp.pt.x()) * scale_x;
            const float ky = static_cast<float>(kp.pt.y()) * scale_y;

            // Horizontal Line
            vertices[index++].set(kx - k_crosshair_radius, ky);
            vertices[index++].set(kx + k_crosshair_radius, ky);
            // Vertical Line
            vertices[index++].set(kx, ky - k_crosshair_radius);
            vertices[index++].set(kx, ky + k_crosshair_radius);
        }
    }

    node->markDirty(QSGNode::DirtyGeometry);
    return node;
}

}