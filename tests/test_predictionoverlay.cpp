#include <QtTest/QtTest>

#include <QSGGeometryNode>

#include <predictionoverlay.h>

class TestablePredictionOverlay : public MTGS::PredictionOverlay
{
public:
    using MTGS::PredictionOverlay::updatePaintNode;
};

class PredictionOverlayTest : public QObject
{
    Q_OBJECT

private slots:
    void createsEmptyGeometryForEmptyPredictions();
    void ignoresZeroSourceDimensions();
    void scalesBoxAndKeypointGeometry();
    void clearsGeometryAfterEmptyUpdate();
};

void PredictionOverlayTest::createsEmptyGeometryForEmptyPredictions()
{
    TestablePredictionOverlay overlay;
    overlay.setWidth(200);
    overlay.setHeight(100);

    QSGNode *node = overlay.updatePaintNode(nullptr, nullptr);
    QVERIFY(node != nullptr);
    QCOMPARE(static_cast<QSGGeometryNode *>(node)->geometry()->vertexCount(), 0);
    delete node;
}

void PredictionOverlayTest::ignoresZeroSourceDimensions()
{
    TestablePredictionOverlay overlay;
    overlay.setWidth(100);
    overlay.setHeight(100);
    MTGS::Prediction prediction;
    prediction.box = QRect(0, 0, 10, 10);
    QList<MTGS::Prediction> predictions { prediction };

    overlay.updatePredictions(std::move(predictions), QSize());

    auto *node = static_cast<QSGGeometryNode *>(overlay.updatePaintNode(nullptr, nullptr));
    QCOMPARE(node->geometry()->vertexCount(), 0);
    delete node;
}

void PredictionOverlayTest::scalesBoxAndKeypointGeometry()
{
    TestablePredictionOverlay overlay;
    overlay.setWidth(200);
    overlay.setHeight(100);

    MTGS::Prediction prediction;
    prediction.box = QRect(10, 5, 20, 10);
    prediction.keypoints = {{{15.0, 10.0}, 1.0f, 0}};
    QList<MTGS::Prediction> predictions { prediction };
    overlay.updatePredictions(std::move(predictions), QSize(100, 50));

    auto *node = static_cast<QSGGeometryNode *>(overlay.updatePaintNode(nullptr, nullptr));
    const auto *vertices = node->geometry()->vertexDataAsPoint2D();
    QCOMPARE(node->geometry()->vertexCount(), 12);
    QCOMPARE(vertices[0].x, 20.0f);
    QCOMPARE(vertices[0].y, 10.0f);
    QCOMPARE(vertices[1].x, 60.0f);
    QCOMPARE(vertices[1].y, 10.0f);
    QCOMPARE(vertices[8].x, 26.0f);
    QCOMPARE(vertices[8].y, 20.0f);
    QCOMPARE(vertices[9].x, 34.0f);
    QCOMPARE(vertices[9].y, 20.0f);
    delete node;
}

void PredictionOverlayTest::clearsGeometryAfterEmptyUpdate()
{
    TestablePredictionOverlay overlay;
    overlay.setWidth(100);
    overlay.setHeight(100);

    MTGS::Prediction prediction;
    prediction.box = QRect(0, 0, 10, 10);
    QList<MTGS::Prediction> predictions { prediction };
    overlay.updatePredictions(std::move(predictions), QSize(100, 100));
    auto *node = static_cast<QSGGeometryNode *>(overlay.updatePaintNode(nullptr, nullptr));
    QCOMPARE(node->geometry()->vertexCount(), 8);

    overlay.updatePredictions({}, QSize(100, 100));
    node = static_cast<QSGGeometryNode *>(overlay.updatePaintNode(node, nullptr));
    QCOMPARE(node->geometry()->vertexCount(), 0);
    delete node;
}

QTEST_MAIN(PredictionOverlayTest)
#include "test_predictionoverlay.moc"