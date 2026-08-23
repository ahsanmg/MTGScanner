#pragma once

#include <QHash>
#include <QList>

#include <ByteTrack/BYTETracker.h>
#include <opencv2/core/types.hpp>

#include <core/frame.hpp>
#include <core/prediction.hpp>

namespace MTGS {

class CardProcessor {
public:
    struct TrackedCard {
        int lostCount = 0;
        int lastBoxArea = 0;
        int retries = 0;
    };

    explicit CardProcessor(int fps = 30, int trackBufferSize = 30, float trackThresh = 0.15f, float highThresh = 0.4f, float matchThresh = 0.8f);
    void process(FramePtr frame);

private:
    QList<Prediction> relateSubPredictions(const QList<Prediction> &predictions);
    // Checks if inner is inside outer by the percentage.
    bool intersects(const QRect &inner, const QRect &outer, float percent = 0.90f);

private:
    QHash<int, TrackedCard> m_trackedCards;
    byte_track::BYTETracker m_tracker;

    int m_maxTimeLost = 30;
};

}