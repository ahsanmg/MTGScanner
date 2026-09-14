#include <QtTest/QtTest>

#include <opencv2/core.hpp>

#include <core/image.hpp>

class ImageTest : public QObject
{
    Q_OBJECT

private slots:
    void handlesEmptyLetterBoxInput();
    void letterBoxesImage();
    void permutesChannelsToPlanarBuffer();
    void scalesAndClipsCoordinates();
    void cropsPerspectiveImage();
    void suppressesOverlappingBoxesByScore();
};

void ImageTest::handlesEmptyLetterBoxInput()
{
    cv::Mat output(2, 2, CV_8UC1, cv::Scalar(1));

    MTGS::letterBox(cv::Mat{}, output, cv::Size(8, 8));

    QVERIFY(output.empty());
}

void ImageTest::letterBoxesImage()
{
    cv::Mat image(2, 4, CV_8UC3, cv::Scalar(10, 20, 30));
    cv::Mat output;

    MTGS::letterBox(image, output, cv::Size(8, 8));

    QCOMPARE(output.size(), cv::Size(8, 8));
    QCOMPARE(output.type(), CV_8UC3);
    QCOMPARE(output.at<cv::Vec3b>(3, 3)[0], static_cast<uchar>(10));
    QCOMPARE(output.at<cv::Vec3b>(0, 0)[0], static_cast<uchar>(114));
}

void ImageTest::permutesChannelsToPlanarBuffer()
{
    cv::Mat image(1, 2, CV_32FC3);
    image.at<cv::Vec3f>(0, 0) = cv::Vec3f(1.0f, 2.0f, 3.0f);
    image.at<cv::Vec3f>(0, 1) = cv::Vec3f(4.0f, 5.0f, 6.0f);
    QList<cv::Mat> batch { image };
    std::vector<float> output(6, 0.0f);

    MTGS::permute(batch, output);

    QCOMPARE(output, std::vector<float>({1.0f, 4.0f, 2.0f, 5.0f, 3.0f, 6.0f}));
}

void ImageTest::scalesAndClipsCoordinates()
{
    cv::Rect2f box(10.0f, 20.0f, 50.0f, 40.0f);
    MTGS::scaleCoords(cv::Size(100, 100), cv::Size(30, 30), box, 2.0f, 2, 4);
    QCOMPARE(box.x, 4.0f);
    QCOMPARE(box.y, 8.0f);
    QCOMPARE(box.width, 25.0f);
    QCOMPARE(box.height, 20.0f);

    MTGS::KeyPoint point { QPointF(10.0, 20.0), 1.0f, 0 };
    MTGS::scaleCoords(cv::Size(100, 100), cv::Size(12, 12), point, 2.0f, 2, 4);
    QCOMPARE(point.pt, QPointF(4.0, 8.0));
}

void ImageTest::cropsPerspectiveImage()
{
    QImage source(20, 20, QImage::Format_RGB888);
    source.fill(Qt::red);
    QImage result;
    const QPolygonF source_points {
        QPointF(0, 0), QPointF(20, 0), QPointF(20, 20), QPointF(0, 20)
    };
    const QPolygonF destination_points {
        QPointF(0, 0), QPointF(10, 0), QPointF(10, 5), QPointF(0, 5)
    };

    MTGS::perspectiveCrop(source, result, source_points, destination_points);

    QCOMPARE(result.size(), QSize(10, 5));
    QCOMPARE(result.pixelColor(5, 2).red(), 255);
}

void ImageTest::suppressesOverlappingBoxesByScore()
{
    const QList<cv::Rect> boxes {
        cv::Rect(0, 0, 10, 10),
        cv::Rect(1, 1, 10, 10),
        cv::Rect(30, 30, 10, 10)
    };
    const QList<float> scores { 0.9f, 0.8f, 0.7f };

    const QList<int> result = MTGS::nmsBBoxes(boxes, scores, 0.0f, 0.5f);

    QCOMPARE(result, QList<int>({0, 2}));
}

QTEST_MAIN(ImageTest)
#include "test_image.moc"