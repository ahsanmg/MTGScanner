#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <numeric>
#include <stdexcept>
#include <filesystem>
#include <functional>

#include <QFile>
#include <QLoggingCategory>

#include <onnxruntime_cxx_api.h>
#include <onnxruntime_c_api.h>
#include <opencv2/core/hal/interface.h>
#include <opencv2/core/types.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/imgproc.hpp>
#include <yaml-cpp/yaml.h>

#include <core/prediction.hpp>
#include "carddetector.h"

namespace MTGS {

// Some extra functions
void letterBox(const cv::Mat& image, cv::Mat& outImage,
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

void permute(const QList<cv::Mat> &batch,
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

void scaleCoords(const cv::Size &resizedImageSize,
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

void scaleCoords(const cv::Size &resizedImageSize,
                const cv::Size &originalImageSize,
                KeyPoint &point,
                float gain, int padX, int padY, bool clip = true)
{
    point.pt.x = std::round((point.pt.x - padX) / gain);
    point.pt.y = std::round((point.pt.y - padY) / gain);

    if (clip) {
        point.pt.x = std::clamp<float>(point.pt.x, 0.0f, originalImageSize.width);
        point.pt.y = std::clamp<float>(point.pt.y, 0.0f, originalImageSize.height);
    }
}

void scaleCoords(const cv::Size &resizedImageSize,
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

Q_STATIC_LOGGING_CATEGORY(logger, "mtgs.card.detector")
CardDetector::CardDetector(QSharedPointer<Ort::Env> env, const CardDetectorConfig &config, Ort::SessionOptions sessionOptions, Ort::MemoryInfo memoryInfo)
    : m_env(env)
    , m_config(config)
{
    if (m_config.path.empty() || !std::filesystem::exists(m_config.path)) {
        qCCritical(logger) << QString("Model path is invalid or the file does not exist: %1").arg(m_config.path);
        throw std::runtime_error("config.path: Model path is invalid or the file does not exist.");
    }

    if (!m_env)
        m_env = QSharedPointer<Ort::Env>::create(ORT_LOGGING_LEVEL_WARNING, "ORT_CARD_POSE");

    if (!memoryInfo)
        memoryInfo = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    m_tensorMemoryInfo = std::move(memoryInfo);

    try {
        if (!sessionOptions) {
            sessionOptions = Ort::SessionOptions{};
            sessionOptions.SetIntraOpNumThreads(2);
            sessionOptions.SetInterOpNumThreads(1);
            sessionOptions.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);

            auto providers = Ort::GetAvailableProviders();
            if (std::find(providers.begin(), providers.end(), "OpenVINOExecutionProvider")
                != providers.end()) {
                std::unordered_map<std::string, std::string> ov_options;
                ov_options["device_type"] = "GPU";
                ov_options["precision"] = "ACCURACY";
                ov_options["num_of_threads"] = "2";
                // ov_options["disable_dynamic_shapes"] = "false";

                sessionOptions.AppendExecutionProvider_OpenVINO_V2(ov_options);
            }

            qCDebug(logger) << "Available providers:" << providers;
        }

#ifdef WIN32
        std::wstring model_path(m_config.path.begin(), m_config.path.end());
#else
        std::string model_path = m_config.path;
#endif
        m_session = Ort::Session(*m_env, model_path.c_str(), sessionOptions);

        // Populate the inputs/outputs
        m_inputNames = m_session.GetInputNames();
        for (size_t i = 0; i < m_session.GetInputCount(); ++i)
            m_inputNamesP.push_back(m_inputNames.at(i).c_str());

        m_outputNames = m_session.GetOutputNames();
        for (size_t i = 0; i < m_session.GetOutputCount(); ++i)
            m_outputNamesP.push_back(m_outputNames.at(i).c_str());

        // Metadata
        const auto metadata = m_session.GetModelMetadata();
        const auto keys = metadata.GetCustomMetadataMapKeysAllocated(m_cpuAllocator);
        for (const auto &key : keys)
            m_modelMetadata[key.get()] = metadata.LookupCustomMetadataMapAllocated(key.get(), m_cpuAllocator).get();


        validateAndFillConfig(m_config);
    } catch (const Ort::Exception& e) {
        throw std::runtime_error(QString("ONNXRuntime error %1: %2").arg(static_cast<int>(e.GetOrtErrorCode())).arg(e.what()).toStdString());
    } catch (const std::exception& e) {
        throw std::runtime_error(QString("Standard exception caught: %1").arg(e.what()).toStdString());
    }
}

QList<QList<Prediction>> CardDetector::predict(const QList<cv::Mat> &batch, float threshold)
{
    if (batch.empty())
        return {};

    QList<QList<Prediction>> predictions_list;
    const int config_batch = m_config.batch.value();
    predictions_list.reserve(batch.size());

    try {
        for(size_t b = 0; b < batch.size();) {
            const size_t sel_end = config_batch < 0                                   // batch is set to -1
                                    ? batch.size()                               // use the whole batch
                                    : std::min<size_t>(batch.size(), b + config_batch);    // else, the specific size
            const size_t sel_size = sel_end - b;

            auto [sel_batch, resized_size, shape] = preProcess(batch, b, sel_size);
            std::vector<float> input_data(std::accumulate(shape.begin(), shape.end(), 1, std::multiplies<int>()));
            permute(sel_batch, input_data);

            Ort::RunOptions run_options;
            std::vector<Ort::Value> input_tensors;
            input_tensors.push_back(
                Ort::Value::CreateTensor(
                    m_tensorMemoryInfo,
                    input_data.data(),
                    input_data.size(),
                    shape.data(),
                    shape.size()
                )
            );

            auto output_tensors = m_session.Run(
                run_options, 
                m_inputNamesP.data(), 
                input_tensors.data(), 
                input_tensors.size(), 
                m_outputNamesP.data(), 
                m_outputNamesP.size()
            );

            QList<QList<Prediction>> predictions = postProcess(batch, b, sel_size, resized_size, output_tensors);
            predictions_list.append(predictions);
            b = sel_end;
        }
    } catch (const std::exception &e) {
        qCCritical(logger) << "Failed inference," << e.what();
    }

    return predictions_list;
}

void CardDetector::draw(cv::Mat &image, const QList<Prediction> &predictions, bool drawBBox, bool drawKeypoints, bool drawSkeletons, float maskAlpha)
{
    if (predictions.empty())
        return;

    const auto& names = m_config.names.value();

    const float font_scale = std::min(image.rows, image.cols) * 0.0008f;
    const int font_thickness = std::max(1, static_cast<int>(std::min(image.rows, image.cols) * 0.002));
    const float scale_factor = std::min(image.rows, image.cols) / 1280.0f;  // reference 1280px size
    const int line_thickness = std::max(2, static_cast<int>(3 * scale_factor));
    const int kpt_radius = std::max(3, static_cast<int>(5 * scale_factor));

    for (const auto& prediction : predictions) {
        if (prediction.classId < 0 || prediction.classId >= m_config.names.value().size())
            continue;

        
        // Draw bounding box
        if (drawBBox) {
            QString label;
            const cv::Scalar& color = m_colors.empty() ? cv::Scalar(0, 0, 255) : m_colors[prediction.classId % m_colors.size()];

            if (names.empty()) {
                label = QString("%1%").arg(static_cast<int>(prediction.confidence * 100));
            } else if (prediction.trackerId == -1) {
                label = QString("%1 - %2%")
                            .arg(names.at(prediction.classId))
                            .arg(static_cast<int>(prediction.confidence * 100));
            } else {
                label = QString("%1 - %2 - %3%")
                            .arg(names.at(prediction.classId))
                            .arg(prediction.trackerId)
                            .arg(static_cast<int>(prediction.confidence * 100));
            }

            cv::rectangle(image, prediction.box, color, 2,  cv::LINE_AA);

            int baseline = 0;
            const cv::Size text_size = cv::getTextSize(label.toStdString(), cv::FONT_HERSHEY_SIMPLEX, font_scale, font_thickness, &baseline);
            const int label_y = std::max(prediction.box.y, text_size.height + 5);
            const cv::Point label_tl(prediction.box.x, label_y - text_size.height - 5);
            const cv::Point label_br(prediction.box.x + text_size.width + 5, label_y + baseline - 5);

            // Label background
            cv::rectangle(image, label_tl, label_br, color, cv::FILLED);
            cv::putText(image, label.toStdString(), cv::Point(prediction.box.x + 2, label_y - 2),
                        cv::FONT_HERSHEY_SIMPLEX, font_scale, cv::Scalar(255, 255, 255),
                        font_thickness, cv::LINE_AA);
        }
        
        const QList<KeyPoint> &kpts = prediction.keypoints;
        const size_t num_kpts = kpts.size();
        if (drawKeypoints) {
            // Draw keypoints
            for (size_t i = 0; i < num_kpts; ++i)
                cv::circle(image, kpts[i].pt, kpt_radius, m_colors[i % m_colors.size()], -1, cv::LINE_AA);
        }

        if (drawSkeletons) {
            // Draw skeleton connections
            const auto &skeleton = m_config.kptSkeleton.value();
            for (size_t j = 0; j < skeleton.size(); ++j) {
                const auto [src, dst] = skeleton[j];
                if (src < num_kpts && dst < num_kpts) {
                    cv::line(image, kpts[src].pt, kpts[dst].pt,
                             m_colors[src % m_colors.size()],
                             line_thickness, cv::LINE_AA);
                }
            }
        }
    }
}

void CardDetector::printModelMetadata()
{
    const Ort::ModelMetadata &metadata = m_session.GetModelMetadata();
    qCDebug(logger) << "Model metadata:";
    qCDebug(logger) << "  Graph Name:" << metadata.GetGraphNameAllocated(m_cpuAllocator).get();
    qCDebug(logger) << "  Graph Description:" << metadata.GetGraphDescriptionAllocated(m_cpuAllocator).get();
    qCDebug(logger) << "  Description:" << metadata.GetDescriptionAllocated(m_cpuAllocator).get();
    qCDebug(logger) << "  Domain:" << metadata.GetDomainAllocated(m_cpuAllocator).get();
    qCDebug(logger) << "  Producer:" << metadata.GetProducerNameAllocated(m_cpuAllocator).get();
    qCDebug(logger) << "  Version:" << metadata.GetVersion();

    qCDebug(logger) << "  Custom Metadata:";
    for (const auto &[key, val] : m_modelMetadata.asKeyValueRange()) {
        qCDebug(logger) << "    " << key << ":" << val;
    }

    qCDebug(logger) << "  Inputs:";
    qCDebug(logger) << "    Name:" << m_inputNames.at(0);
    qCDebug(logger) << "    Shape:" << m_session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();

    qCDebug(logger) << "  Outputs:";
    qCDebug(logger) << "    Name:" << m_outputNames.at(0);
    qCDebug(logger) << "    Shape:" << m_session.GetOutputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape() << "\n";
}

bool CardDetector::hasDynamicBatch() 
{
    const auto &inputs = m_session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    if (inputs.size() == 4
        && inputs.at(0) == -1) {
        return true;
    }

    return false;
}

bool CardDetector::hasDynamicShape() 
{
    const auto &inputs = m_session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    if (inputs.size() == 4
        && (inputs.at(2) == -1
            || inputs.at(3) == -1)) {
        return true;
    }

    return false;
}

void CardDetector::setColors(const QList<cv::Scalar> &colors)
{
    m_colors = colors;
}

bool CardDetector::validateAndFillConfig(CardDetectorConfig &config)
{
    // Pull out yolo specific metadata
    const auto &metadata = m_modelMetadata;
    if (m_modelMetadata.value("task", "unknown") != "pose") {
        throw std::runtime_error("An unsupported model task detected. Please use a correct pose model.");
    }

    // Common
    if (!m_config.stride && metadata.contains("stride"))
        m_config.stride = std::stoi(metadata.value("stride"));

    auto shape = m_session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    auto model_batch = static_cast<int>(shape.at(0));
    if (!m_config.batch) {
        // User didn't provide batch
        if (metadata.contains("batch")){
            // Assign what model metadata has
            m_config.batch = std::stoi(metadata.value("batch"));
        } else {
            // Fallback to 1 (dynamic) or input shape (fixed)
            m_config.batch = hasDynamicBatch() ? 1 : model_batch;
        }
    } else {
        // Fixed shape? compare with input shape. if mismatch, enforce
        if (!hasDynamicBatch()
            && model_batch != m_config.batch.value())
            m_config.batch = model_batch;
    }

    if (!m_config.imgsz && metadata.contains("imgsz")) {
        if (YAML::Node node = YAML::Load(metadata.value("imgsz")))
            m_config.imgsz = node.as<std::array<int, 2>>();
    }

    YAML::Node node;
    if (!m_config.names
        && metadata.contains("names")
        && (node = YAML::Load(metadata.value("names")))) {
        m_config.names = node.as<std::unordered_map<int, std::string>>();
    } else {
        throw std::runtime_error("No labels/names/classes were found. Please provide labels for this model!");
    }

    YAML::Node kpt_shape;
    if (!m_config.kptShape) {
        if (metadata.contains("kpt_shape")
            && (kpt_shape = YAML::Load(metadata.value("kpt_shape")))) {
            m_config.kptShape = kpt_shape.as<std::array<int, 2>>();
        } else {
            m_config.kptShape = {4, 3};
        }
    } 

    if (!m_config.kptSkeleton) {
        if (metadata.contains("kpt_skeleton")) {
            if (YAML::Node kpt_skeleton = YAML::Load(metadata.value("kpt_skeleton")))
                m_config.kptSkeleton = kpt_skeleton.as<std::vector<std::pair<int, int>>>();
        } else {
            m_config.kptSkeleton = { {0, 1}, {1, 2}, {2, 3}, {3, 0} };
        }
    }

    return true;
}

std::tuple<QList<cv::Mat>, cv::Size, std::vector<int64_t>> CardDetector::preProcess(const QList<cv::Mat> &batch, int batchIndx, int batchSize)
{
    auto shape = m_session.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo().GetShape();
    int max_h = shape.at(2);
    int max_w = shape.at(3);

    if (hasDynamicShape()) {
        int model_stride = m_config.stride.value_or(32);

        if (max_h == -1) {
            if (m_config.imgsz) {
                max_h = m_config.imgsz->at(0);
            } else {
                for (size_t s = 0; s < batchSize; ++s)
                    max_h = std::max(max_h, batch[batchIndx + s].rows);

                if (max_h % model_stride != 0)
                    max_h = ((max_h / model_stride) + 1) * model_stride;
            }

            shape[2] = max_h;
        }

        if (max_w == -1) {
            if (m_config.imgsz) {
                max_w = m_config.imgsz->at(1);
            } else {
                for (size_t s = 0; s < batchSize; ++s)
                    max_w = std::max(max_w, batch[batchIndx + s].cols);

                if (max_w % model_stride != 0)
                    max_w = ((max_w / model_stride) + 1) * model_stride;
            }

            shape[3] = max_w;
        }
    }
    shape[0] = batchSize;

    QList<cv::Mat> res_batch;
    const cv::Size new_size(shape.at(3), shape.at(2));
    for (size_t s = 0; s < batchSize; ++s) {
        cv::Mat img;
        letterBox(batch[batchIndx + s], img, new_size);
        cv::cvtColor(img, img, cv::COLOR_BGR2RGB);
        img.convertTo(img, CV_32FC1, 1.0f / 255.0f);

        res_batch.append(img);
    }

    return { res_batch, new_size, shape };
}

QList<QList<Prediction>> CardDetector::postProcess(const QList<cv::Mat> &batch, int batchIndx, int batchSize, cv::Size resizedSize, const std::vector<Ort::Value> &outputTensors, float threshold)
{
    const auto &tensor = outputTensors.at(0);
    const auto &shape0 = tensor.GetTensorTypeAndShapeInfo().GetShape();
    const float* output0_data = tensor.GetTensorData<float>();

    /*
        Expected shape should be:
            0: N (batch size)
            1: cx,cy,w,h + classes_scores + kpts (x,y,v)
            2: num_predictions

        // Its [B, F, D], not [B, D, F]. We hover over each feature's detections

        * D (detections) being the fastest-varying, then F (features) and then B (batch)
         * The structure of each batch is laid out something like this for [B, F, D]
            Batch 0
                Feature 0
                    Detections 0, 1, ... D-1
                Feature 1
                    Detections 0, 1, ... D-1
                ...
            Batch 1
                Feature 0
                    Detections 0, 1, ... D-1
                Feature 1
                    Detections 0, 1, ... D-1
                ...
            ...
    */

    const size_t out_batch_size = shape0.at(0);
    const size_t out_num_features = shape0.at(1);
    const size_t out_num_detections = shape0.at(2);
    const int num_keypoints = m_config.kptShape->at(0);
    const int num_kp_features = m_config.kptShape->at(1);
    const int out_num_classes = out_num_features - 4 - num_keypoints * num_kp_features;

    QList<QList<Prediction>> results_list;
    results_list.reserve(batchSize);

    for (size_t b = 0; b < batchSize; ++b) {
        const float *batch_offsetptr = output0_data + b * (out_num_features * out_num_detections); // Jumps b * features * predictions for batch b.
        const float *card_front_row = batch_offsetptr + 4 * out_num_detections;
        const float *title_row =      batch_offsetptr + 5 * out_num_detections;
        const float *card_back_row =  batch_offsetptr + 6 * out_num_detections;

        QList<cv::Rect2f> boxes;
        QList<cv::Rect> nms_boxes;
        QList<QList<KeyPoint>> keypoints_list;
        QList<float> scores;
        QList<int> class_ids;

        for (size_t col = 0; col < out_num_detections; ++col) {
            float cf_score = card_front_row[col];
            float tit_score = title_row[col];
            float cb_score = card_back_row[col];

            int class_id = 0;
            float max_score = cf_score;
            if (tit_score > max_score) { max_score = tit_score; class_id = 1; }
            if (cb_score > max_score) { max_score = cb_score; class_id = 2; }

            if (max_score < threshold)
                continue;

            const float cx = batch_offsetptr[0 * out_num_detections + col];
            const float cy = batch_offsetptr[1 * out_num_detections + col];
            const float w  = batch_offsetptr[2 * out_num_detections + col];
            const float h  = batch_offsetptr[3 * out_num_detections + col];
            cv::Rect2f coords(cx - w / 2.0f, cy - h / 2.0f, w, h);

            cv::Rect nms_box = coords;
            nms_box.x += class_id * 7880; // arbitrary offset to differentiate classes
            nms_box.y += class_id * 7880;

            QList<KeyPoint> keypoints;
            for (int k = 0; k < num_keypoints; ++k) {
                KeyPoint keypoint;
                const int kp_offset = 4 + out_num_classes + k * num_kp_features;
                keypoint.pt.x       = batch_offsetptr[(0 + kp_offset) * out_num_detections + col];
                keypoint.pt.y       = batch_offsetptr[(1 + kp_offset) * out_num_detections + col];
                keypoint.visibility = batch_offsetptr[(2 + kp_offset) * out_num_detections + col];
                keypoints.push_back(keypoint);
            }

            boxes.emplace_back(coords);
            nms_boxes.emplace_back(nms_box);
            keypoints_list.emplace_back(keypoints);
            scores.emplace_back(max_score);
            class_ids.emplace_back(class_id);
        }

        // Apply Non-Maximum Suppression (NMS) to eliminate redundant detections
        const QList<int> indices = nmsBBoxes(nms_boxes, scores, threshold, m_config.iouThreshold.value_or(0.4f));

        const cv::Size orig_size(batch[batchIndx + b].cols, batch[batchIndx + b].rows);
        const float gain = std::min(static_cast<float>(resizedSize.height) / orig_size.height, static_cast<float>(resizedSize.width) / orig_size.width);
        const int pad_x = std::round((resizedSize.width - orig_size.width * gain) / 2.0f);
        const int pad_y = std::round((resizedSize.height - orig_size.height * gain) / 2.0f);

        const auto &labels = m_config.names.value();
        QList<Prediction> results;
        results.reserve(indices.size());
        for (const int idx : indices) {
            scaleCoords(resizedSize, orig_size, boxes[idx], gain, pad_x, pad_y);
            scaleCoords(resizedSize, orig_size, keypoints_list[idx], gain, pad_x, pad_y);

            Prediction prediction;
            prediction.box = boxes[idx];
            prediction.keypoints = keypoints_list[idx];
            prediction.confidence = scores[idx];
            prediction.classId = class_ids[idx];
            prediction.className = QString::fromStdString(labels.at(prediction.classId));

            results.emplace_back(prediction);
        }

        results_list.emplace_back(results);
    }

    return results_list;
}

} // namespace MTGS