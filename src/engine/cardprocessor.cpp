#include "cardprocessor.h"

#include <cstddef>
#include <vector>
#include <utility>

#include <QtGui/QImage>

#include <ByteTrack/Object.h>
#include <ByteTrack/Rect.h>
#include <ByteTrack/BYTETracker.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>
#include <opencv2/imgproc.hpp>

#include <core/frame.hpp>
#include <core/prediction.hpp>
#include <core/constants.hpp>

namespace MTGS {

namespace bt = byte_track;
CardProcessor::CardProcessor(int fps, int trackBufferSize, float trackThresh, float highThresh, float matchThresh)
    : m_tracker(fps, trackBufferSize, trackThresh, highThresh, matchThresh)
    , m_maxTimeLost(fps / 30.0f * trackBufferSize)
{}

void CardProcessor::process(FramePtr frame)
{
    frame->predictions = relateSubPredictions(frame->predictions);

    // Increase lost count for tracked cards
    for (auto &tc : m_trackedCards)
        tc.lostCount++;

    std::vector<int> tracked_indices;
    std::vector<bt::Object> objects;
    for (size_t i = 0; i < frame->predictions.size(); ++i) {
        const auto &card = frame->predictions.at(i);
        byte_track::Rect<float> box { 
            static_cast<float>(card.box.x),
            static_cast<float>(card.box.y),
            static_cast<float>(card.box.width),
            static_cast<float>(card.box.height)
        };
        objects.emplace_back(box, card.classId, card.confidence);
        tracked_indices.push_back(i);
    }

    const auto outputs = m_tracker.update(objects);

    for (size_t i = 0; i < outputs.size(); ++i) {
        const auto output = outputs.at(i);
        auto &card = frame->predictions[tracked_indices.at(i)];
        card.trackerId = output->getTrackId();

        // Perspective Crop titles/nameplates of the card at least MAX_CROP_RETRIES and when
        // the area actually changes (i.e. the card is closing to the camera).
        TrackedCard &tracked_card = m_trackedCards[card.trackerId];
        tracked_card.lostCount = 0; // reset
        bool can_crop = tracked_card.retries < MAX_CROP_RETRIES
                        && card.box.area() > tracked_card.lastBoxArea * 1.35f;
        if (!can_crop || !card.subPredictions)
            continue;

        if (!card.crops)
            card.crops = QList<cv::Mat>();

        tracked_card.retries++;
        tracked_card.lastBoxArea = card.box.area();

        for (const auto &np : card.subPredictions.value()) {
            // NOTE: We assume all are titles
            const auto &points = np.keypoints;
            std::array<cv::Point2f, 4> src_points = {
                points.at(0).pt, points.at(1).pt,
                points.at(2).pt, points.at(3).pt
            };

            const float exp_w = static_cast<float>(TITLE_WIDTH);
            const float exp_h = static_cast<float>(TITLE_HEIGHT);
            std::array<cv::Point2f, 4> dst_points = {
                cv::Point2f { 0.0f,  0.0f },    // tl
                cv::Point2f { exp_w, 0.0f },    // tr
                cv::Point2f ( exp_w, exp_h ),   // br
                cv::Point2f { 0.0f,  exp_h }    // bl
            };
    
            const QImage &img = frame->frameImg;
            cv::Mat nameplate(img.height(), img.width(), CV_8UC3, const_cast<uchar*>(img.bits()), img.bytesPerLine());;
            perspectiveCrop(nameplate, nameplate, src_points, dst_points);
            card.crops->append(nameplate);
        }
    }

    // Remove the lost
    for (const auto &[key, tc] : m_trackedCards.asKeyValueRange()) {
        if (tc.lostCount > m_maxTimeLost)
            m_trackedCards.remove(key);
    }
}

void CardProcessor::perspectiveCrop(const cv::Mat &img, cv::Mat &res, const std::array<cv::Point2f, 4> &srcPoints, const std::array<cv::Point2f, 4> &dstPoints)
{
    if (img.empty() || srcPoints.empty() || dstPoints.empty())
        return;

    const int h = dstPoints.at(3).y - dstPoints.at(0).y;
    const int w = dstPoints.at(1).x - dstPoints.at(0).x;

    cv::Mat tr = cv::getPerspectiveTransform(srcPoints, dstPoints);
    cv::warpPerspective(img, res, tr, cv::Size(w, h));
}

QList<Prediction> CardProcessor::relateSubPredictions(const QList<Prediction> &predictions)
{
    QHash<QString, QList<int>> separate_indices;
    for (size_t i = 0; i < predictions.size(); ++i)
        separate_indices[predictions.at(i).className].push_back(i);

    auto &titles_indices = separate_indices["title"];
    if (titles_indices.empty())
        return predictions;

    QList<Prediction> final_predictions;
    for (const auto &f : separate_indices["card_front"]) {
        Prediction front = predictions.at(f);
        for (const auto &t : titles_indices) {
            Prediction title = predictions.at(t);
            if (!intersects(title.box, front.box, MIN_INTERSECTION_RATIO))
                continue;

                // Title is inside
            if (!front.subPredictions) 
                front.subPredictions = QList<Prediction>();

            front.subPredictions->emplace_back(std::move(title));
            titles_indices.removeOne(t);
        }
        final_predictions.emplace_back(std::move(front));
    }

    for (const auto &b : separate_indices["card_back"])
        final_predictions.emplace_back(std::move(predictions.at(b)));

    return final_predictions;
}

bool CardProcessor::intersects(const cv::Rect &inner, const cv::Rect &outer, float percent)
{
    const double inner_area = inner.area();
    if (inner_area == 0)
        return false;

    cv::Rect intersection = inner & outer;
    const double intersection_area = intersection.area();
    const double overlap_ratio = intersection_area / inner_area;
    return overlap_ratio >= percent;
}

}