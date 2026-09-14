#include <QtTest/QtTest>

#include <QImage>

#include <camera/cameracapture.h>

class CameraCaptureTest : public QObject
{
    Q_OBJECT

private slots:
    void ignoresInvalidFrames();
    void countsValidFrameAsCapturedAndSkippedWithoutGateway();
};

void CameraCaptureTest::ignoresInvalidFrames()
{
    FramesPerSecond fps;
    FramesPerSecond skipped_fps;
    MTGS::CameraCapture capture(QStringLiteral("channel"), QStringLiteral("camera"), fps, skipped_fps);

    capture.onVideoFrameChanged(QVideoFrame{});

    QCOMPARE(fps.eventCount(), 0);
    QCOMPARE(skipped_fps.eventCount(), 0);
}

void CameraCaptureTest::countsValidFrameAsCapturedAndSkippedWithoutGateway()
{
    FramesPerSecond fps;
    FramesPerSecond skipped_fps;
    MTGS::CameraCapture capture(QStringLiteral("dummy_channel_id"), QStringLiteral("dummy_camera_id"), fps, skipped_fps);
    QImage image(8, 8, QImage::Format_ARGB32);
    image.fill(Qt::green);

    capture.onVideoFrameChanged(QVideoFrame(image));

    QCOMPARE(fps.eventCount(), 1);
    QCOMPARE(skipped_fps.eventCount(), 1);
}

QTEST_MAIN(CameraCaptureTest)
#include "test_cameracapture.moc"