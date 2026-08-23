#pragma once

#include <qlist.h>
#include <string>
#include <vector>
#include <optional>
#include <unordered_map>

#include <QMap>
#include <QList>
#include <QSharedPointer>

#include <onnxruntime_c_api.h>
#include <onnxruntime_cxx_api.h>
#include <opencv2/core/mat.hpp>
#include <opencv2/core/types.hpp>

#include <core/prediction.hpp>

namespace MTGS {

struct CardDetectorConfig {
    std::string path;
    std::optional<std::string> name;
    std::optional<std::string> version;
    std::optional<int> stride;
    std::optional<int> batch;
    std::optional<int> channels;
    std::optional<std::array<int, 2>> imgsz;
    std::optional<std::unordered_map<int, std::string>> names;
    std::optional<float> iouThreshold;
    std::optional<std::array<int, 2>> kptShape;
    std::optional<std::unordered_map<int, std::vector<int>>> kptNames;
    std::optional<std::vector<std::pair<int, int>>> kptSkeleton;
};

class CardDetector {
public:
    explicit CardDetector(QSharedPointer<Ort::Env> env, const CardDetectorConfig &config, Ort::SessionOptions sessionOptions, Ort::MemoryInfo memoryInfo);
    QList<QList<Prediction>> predict(const QList<QImage> &batch, float threshold = 0.4f);
    void draw(QImage &image, const QList<Prediction> &predictions, bool drawBBox = true, bool drawKeypoints = true, bool drawSkeletons = true, float maskAlpha = 0.3f);
    void printModelMetadata();

    bool hasDynamicBatch();
    bool hasDynamicShape();

    void setColors(const QList<QColor> &colors);

private:
    bool validateAndFillConfig(CardDetectorConfig &config);
    std::tuple<QList<cv::Mat>, cv::Size, std::vector<int64_t>> preProcess(const QList<QImage> &batch, int batchIndx, int batchSize);
    QList<QList<Prediction>> postProcess(const QList<QImage> &batch, int batchIndx, int batchSize, cv::Size resizedSize, const std::vector<Ort::Value> &outputTensor, float threshold = 0.4f);

private:
    CardDetectorConfig m_config;
    QSharedPointer<Ort::Env> m_env;
    Ort::Session m_session{ nullptr };
    Ort::MemoryInfo m_tensorMemoryInfo { nullptr };
    Ort::AllocatorWithDefaultOptions m_cpuAllocator;

    std::vector<std::string> m_inputNames;
    std::vector<std::string> m_outputNames;
    std::vector<const char *> m_inputNamesP;
    std::vector<const char *> m_outputNamesP;
    QMap<std::string, std::string> m_modelMetadata;

    QList<QColor> m_colors;
};

} // namespace MTGSs