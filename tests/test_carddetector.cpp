#include <QtTest/QtTest>

#include <engine/carddetector.h>

class CardDetectorTest : public QObject
{
    Q_OBJECT

private slots:
    void rejectsEmptyModelPath();
    void rejectsMissingModelPath();
};

void CardDetectorTest::rejectsEmptyModelPath()
{
    MTGS::CardDetectorConfig config;

    QVERIFY_EXCEPTION_THROWN(
        MTGS::CardDetector(nullptr, config, Ort::SessionOptions{nullptr}, Ort::MemoryInfo{nullptr}),
        std::runtime_error);
}

void CardDetectorTest::rejectsMissingModelPath()
{
    MTGS::CardDetectorConfig config;
    config.path = "/path/that/does/not/exist/model.onnx";

    QVERIFY_EXCEPTION_THROWN(
        MTGS::CardDetector(nullptr, config, Ort::SessionOptions{nullptr}, Ort::MemoryInfo{nullptr}),
        std::runtime_error);
}

QTEST_MAIN(CardDetectorTest)
#include "test_carddetector.moc"