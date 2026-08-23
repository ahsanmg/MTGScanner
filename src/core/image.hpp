#pragma once

#include <QtCore/QFile>
#include <QtCore/QSettings>
#include <QtCore/QStringView>
#include <QtGui/QIcon>
#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtGui/QPolygon>
#include <QtSvg/QSvgRenderer>
#include <QtMultimedia/QVideoFrame>
#include <QtMultimedia/QVideoFrameFormat>

#include <opencv2/core/hal/interface.h>
#include <opencv2/core/types.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <prediction.hpp>

namespace MTGS {

inline QIcon indexedWindowIcon(const QString &path, int index)
{
    QFile svg(path);
    if (!svg.open(QIODevice::ReadOnly))
        return QIcon(path);

    QByteArray svg_data = svg.readAll();
    svg_data.replace("{{I}}", QByteArray::number(index));

    QIcon icon;
    QSvgRenderer renderer(svg_data);
    for (int size : {16, 32, 48, 64, 128, 256}) {
        QPixmap pixmap(size, size);
        pixmap.fill(Qt::transparent);

        QPainter painter(&pixmap);
        renderer.render(&painter);
        painter.end();

        icon.addPixmap(pixmap);
    }

    return icon;
}

inline void perspectiveCrop(const cv::Mat &img, cv::Mat &res, const std::array<cv::Point2f, 4> &srcPoints, const std::array<cv::Point2f, 4> &dstPoints)
{
    if (img.empty() || srcPoints.empty() || dstPoints.empty())
        return;

    const int h = dstPoints.at(3).y - dstPoints.at(0).y;
    const int w = dstPoints.at(1).x - dstPoints.at(0).x;

    cv::Mat tr = cv::getPerspectiveTransform(srcPoints, dstPoints);
    cv::warpPerspective(img, res, tr, cv::Size(w, h));
}

inline void perspectiveCrop(const QImage &img, QImage &res,
    const QPolygonF &src,
    const QPolygonF &dst)
{
    if (img.isNull())
        return;

    const int w = qRound(dst[1].x() - dst[0].x());
    const int h = qRound(dst[3].y() - dst[0].y());
    if (w <= 0 || h <= 0)
        return;

    QTransform transform;
    if (!QTransform::quadToQuad(src, dst, transform))
        return;

    res = QImage(w, h, QImage::Format_RGB888);
    res.fill(Qt::black);

    QPainter painter(&res);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setTransform(transform);
    painter.drawImage(0, 0, img);
}

inline void letterBox(const cv::Mat& image, cv::Mat& outImage,
                       const cv::Size& newShape,
                       const cv::Scalar& color = cv::Scalar(114, 114, 114),
                       const bool scale = true)
{
    float ratio = std::min(static_cast<float>(newShape.height) / image.rows,
                           static_cast<float>(newShape.width) / image.cols);

    if (!scale)
        ratio = std::min(ratio, 1.0f);

    const cv::Size size_unpdd(std::round(image.cols * ratio), std::round(image.rows * ratio));

    const int pad_hori = newShape.width - size_unpdd.width;
    const int pad_vert = newShape.height - size_unpdd.height;
    const int pad_top = pad_vert / 2;
    const int pad_bottom = pad_vert - pad_top;
    const int pad_left = pad_hori / 2;
    const int pad_right = pad_hori - pad_left;

    cv::resize(image, outImage, size_unpdd);
    cv::copyMakeBorder(outImage, outImage, pad_top, pad_bottom, pad_left, pad_right, cv::BORDER_CONSTANT, color);
}

inline void permute(const QList<cv::Mat> &batch,
                    std::vector<float> &buffer)
{
    for (size_t b = 0; b < batch.size(); ++b) {
        const cv::Mat img = batch[b];
        const int height = img.rows;
        const int width = img.cols;
        const int channels = img.channels();
        float *batch_offset = buffer.data() + b * channels * height * width;

        // Split and permute at once
        std::vector<cv::Mat> out_channels(channels);
        for (int c = 0; c < channels; ++c)
            out_channels[c] = cv::Mat(height, width, CV_32FC1, batch_offset + c * height * width);

        cv::split(img, out_channels);
    }
}

inline void scaleCoords(const cv::Size &resizedImageSize,
                const cv::Size &originalImageSize,
                cv::Rect2f &coords,
                float gain, int padX, int padY, bool clip = true)
{
    coords.x = std::round((coords.x - padX) / gain);
    coords.y = std::round((coords.y - padY) / gain);
    coords.width = std::round(coords.width / gain);
    coords.height = std::round(coords.height / gain);

    if (clip) {
        coords.x = std::clamp<float>(coords.x, 0.0f, originalImageSize.width);
        coords.y = std::clamp<float>(coords.y, 0.0f, originalImageSize.height);
        coords.width = std::clamp<float>(coords.width, 0.0f, originalImageSize.width - coords.x);
        coords.height = std::clamp<float>(coords.height, 0.0f, originalImageSize.height - coords.y);
    }
}

inline void scaleCoords(const cv::Size &resizedImageSize,
                const cv::Size &originalImageSize,
                KeyPoint &point,
                float gain, int padX, int padY, bool clip = true)
{
    point.pt.setX(std::round((point.pt.x() - padX) / gain));
    point.pt.setY(std::round((point.pt.y() - padY) / gain));

    if (clip) {
        point.pt.setX(std::clamp<float>(point.pt.x(), 0.0f, originalImageSize.width));
        point.pt.setY(std::clamp<float>(point.pt.y(), 0.0f, originalImageSize.height));
    }
}

inline void scaleCoords(const cv::Size &resizedImageSize,
                const cv::Size &originalImageSize,
                QList<KeyPoint> &keypoints,
                float gain, int padX, int padY, bool clip = true)
{
    for (auto &point : keypoints)
        scaleCoords(resizedImageSize, originalImageSize, point, gain, padX, padY, clip);
}

inline QList<int> nmsBBoxes(const QList<cv::Rect>& boxes,
                                   const QList<float>& scores,
                                   const float scoreThreshold,
                                   const float iouThreshold)
{

    QList<int> result_indices;
    const size_t num_boxes = boxes.size();
    if (num_boxes < 1)
        return result_indices;

    // Filter and sort based on scores
    std::vector<int> sorted_indices(num_boxes, 0);
    sorted_indices.reserve(num_boxes);
    for (size_t i = 0; i < num_boxes; ++i)
        sorted_indices[i] = static_cast<int>(i);

    std::sort(sorted_indices.begin(), sorted_indices.end(),
              [&scores](int idx1, int idx2) {
                  return scores[idx1] > scores[idx2];
              });

    // Precompute box areas
    std::vector<float> areas(num_boxes, 0.0f);
    for (size_t i = 0; i < num_boxes; ++i) {
        areas[i] = boxes[i].width * boxes[i].height;
    }

    // Suppression mask to mark suppressed boxes.
    std::vector<bool> suppressed(num_boxes, false);

    // Suppress sorted boxes with high IoU
    for (size_t i = 0; i < sorted_indices.size(); ++i) {
        const int current_idx = sorted_indices[i];
        if (suppressed[current_idx]) {
            continue;
        }

        // Select the current box as a valid detection
        result_indices.push_back(current_idx);

        const cv::Rect& current_box = boxes[current_idx];
        const float x1_max = current_box.x;
        const float y1_max = current_box.y;
        const float x2_max = current_box.x + current_box.width;
        const float y2_max = current_box.y + current_box.height;
        const float area_current = areas[current_idx];

        // Compare IoU of the current box with the rest
        for (size_t j = i + 1; j < sorted_indices.size(); ++j) {
            int compare_idx = sorted_indices[j];
            if (suppressed[compare_idx]) {
                continue;
            }

            const cv::Rect& compare_box = boxes[compare_idx];
            const float x1 = std::max(x1_max, static_cast<float>(compare_box.x));
            const float y1 = std::max(y1_max, static_cast<float>(compare_box.y));
            const float x2 = std::min(x2_max, static_cast<float>(compare_box.x + compare_box.width));
            const float y2 = std::min(y2_max, static_cast<float>(compare_box.y + compare_box.height));

            const float inter_width = x2 - x1;
            const float inter_height = y2 - y1;

            if (inter_width <= 0 || inter_height <= 0) {
                continue;
            }

            const float intersection = inter_width * inter_height;
            const float unionArea = area_current + areas[compare_idx] - intersection;
            const float iou = (unionArea > 0.0f) ? (intersection / unionArea) : 0.0f;

            if (iou > iouThreshold) {
                suppressed[compare_idx] = true;
            }
        }
    }

    return result_indices;
}

} // MTGS