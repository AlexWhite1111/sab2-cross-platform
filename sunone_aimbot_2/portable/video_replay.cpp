#include "detector/postProcess.h"
#include "nodes/node_graph.h"

#include <onnxruntime_cxx_api.h>
#include <opencv2/dnn.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

class Model
{
public:
    Model(const std::filesystem::path& path, bool coreml)
    {
        Ort::SessionOptions options;
        if (coreml)
            options.AppendExecutionProvider("CoreML", {{"ModelFormat", "MLProgram"}});
        session_ = Ort::Session(environment_, path.c_str(), options);
        if (session_.GetInputCount() != 1 || session_.GetOutputCount() != 1)
            throw std::runtime_error("Replay requires an ONNX model with one input and one output.");
        const auto inputType = session_.GetInputTypeInfo(0);
        const auto input = inputType.GetTensorTypeAndShapeInfo();
        shape_ = input.GetShape();
        if (input.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
            shape_.size() != 4 || (shape_[0] != 1 && shape_[0] != -1) || shape_[1] != 3 ||
            shape_[2] <= 0 || shape_[2] > 8192 || shape_[3] <= 0 || shape_[3] > 8192)
            throw std::runtime_error("Expected a float32 NCHW model with three channels and fixed spatial dimensions.");
        shape_[0] = 1;
        Ort::AllocatorWithDefaultOptions allocator;
        inputName_ = session_.GetInputNameAllocated(0, allocator).get();
        outputName_ = session_.GetOutputNameAllocated(0, allocator).get();
    }

    NodeGraph::DetectionStream infer(const NodeGraph::FrameStream& packet, const NodeGraph::Node& node)
    {
        const auto& frame = *std::static_pointer_cast<const cv::Mat>(packet->storage);
        const cv::Size modelSize(static_cast<int>(shape_[3]), static_cast<int>(shape_[2]));
        cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0 / 255.0, modelSize, {}, true, false);
        auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        auto input = Ort::Value::CreateTensor<float>(
            memory, blob.ptr<float>(), blob.total(), shape_.data(), shape_.size());
        const char* inputNames[] = {inputName_.c_str()};
        const char* outputNames[] = {outputName_.c_str()};
        auto outputs = session_.Run(Ort::RunOptions{nullptr}, inputNames, &input, 1, outputNames, 1);
        auto& output = outputs.front();
        const auto info = output.GetTensorTypeAndShapeInfo();
        const auto shape = info.GetShape();
        if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT ||
            shape.size() != 3 || shape[0] != 1 ||
            shape[1] <= 0 || shape[1] > std::numeric_limits<int>::max() ||
            shape[2] <= 0 || shape[2] > std::numeric_limits<int>::max())
            throw std::runtime_error("Expected a float32 detection tensor of shape [1, rows, columns].");

        const auto classes = static_cast<int>(NodeGraph::parameter(node, "class_count"));
        const auto detected = postProcessYoloDML(
            output.GetTensorData<float>(), {shape[1], shape[2]}, classes > 0 ? classes : static_cast<int>(shape[1]) - 4,
            static_cast<float>(NodeGraph::parameter(node, "confidence_threshold")),
            static_cast<float>(NodeGraph::parameter(node, "nms_threshold")),
            static_cast<int>(NodeGraph::parameter(node, "max_detections")));

        const float scaleX = static_cast<float>(frame.cols) / modelSize.width;
        const float scaleY = static_cast<float>(frame.rows) / modelSize.height;
        auto result = std::make_shared<NodeGraph::DetectionFrame>();
        result->version = packet->version;
        result->generation = packet->generation;
        result->timestampSeconds = packet->timestampSeconds;
        result->width = packet->width;
        result->height = packet->height;
        result->modelName = NodeGraph::textParameter(node, "model_path");
        result->fresh = true;
        result->objects.reserve(detected.size());
        for (const auto& detection : detected)
        {
            const auto& box = detection.box;
            result->objects.push_back({
                std::trunc(box.x * scaleX), std::trunc(box.y * scaleY),
                std::trunc(box.width * scaleX), std::trunc(box.height * scaleY),
                detection.classId, detection.confidence});
        }
        return result;
    }

private:
    Ort::Env environment_{ORT_LOGGING_LEVEL_WARNING, "SAB2 replay"};
    Ort::Session session_{nullptr};
    std::vector<int64_t> shape_;
    std::string inputName_;
    std::string outputName_;
};

void report(const NodeGraph::DetectionStream& detections, cv::Mat& preview,
    std::int64_t index, double timestamp, double processingMs)
{
    if (detections->objects.empty())
        std::cout << index << ',' << timestamp << "," << processingMs << ",,,,,,\n";

    for (const auto& detection : detections->objects)
    {
        std::cout << index << ',' << timestamp << ',' << processingMs << ',' << detection.classId
            << ',' << detection.confidence << ',' << detection.x << ',' << detection.y
            << ',' << detection.width << ',' << detection.height << '\n';
        if (!preview.empty())
        {
            const cv::Rect box(cv::Point(cvRound(detection.x), cvRound(detection.y)),
                cv::Size(cvRound(detection.width), cvRound(detection.height)));
            cv::rectangle(preview, box, cv::Scalar(0, 255, 0), 2);
        }
    }
}

void replay(const std::string& videoPath, const std::string& modelPath, bool coreml, bool preview)
{
    cv::VideoCapture video(videoPath);
    if (!video.isOpened())
        throw std::runtime_error("Cannot open video file: " + videoPath);
    Model model(modelPath, coreml);

    NodeGraph::Service graph;
    auto workspace = graph.workspace();
    for (auto& page : workspace.graphs)
        if (page.kind == NodeGraph::GraphKind::Vision)
            for (auto& node : page.nodes)
                if (node.kind == NodeGraph::NodeKind::Inference)
                {
                    node.textParameters["model_path"] = modelPath;
                    node.textParameters["backend"] = coreml ? "CoreML" : "CPU";
                }
    std::string error;
    if (!graph.publish(workspace, error))
        throw std::runtime_error(error);

    const double fps = video.get(cv::CAP_PROP_FPS);
    NodeGraph::Context context;
    context.captureFps = std::isfinite(fps) && fps > 0 ? fps : 0;
    context.infer = [&model](int, const auto& packet, const auto& node) { return model.infer(packet, node); };
    std::cout << "frame,seconds,process_ms,class_id,confidence,x,y,width,height\n" << std::setprecision(9);
    std::int64_t index = 0;
    while (true)
    {
        cv::Mat frame;
        if (!video.read(frame))
            break;
        const auto started = Clock::now();
        const auto packet = std::make_shared<NodeGraph::FrameData>();
        packet->storage = std::make_shared<cv::Mat>(frame);
        packet->width = frame.cols;
        packet->height = frame.rows;
        packet->version = index;
        packet->generation = 1;
        packet->timestampSeconds = context.captureFps > 0 ? index / context.captureFps : 0;
        context.elapsedSeconds = packet->timestampSeconds;
        context.center = {frame.cols * 0.5, frame.rows * 0.5};
        context.frame = packet;
        const auto result = graph.evaluateVision(context);
        if (!result.valid || !result.visionDetections)
            throw std::runtime_error("The upstream Vision graph did not publish a detection stream.");
        const double ms = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
        cv::Mat annotated = preview ? frame.clone() : cv::Mat{};
        report(result.visionDetections, annotated, index++, packet->timestampSeconds, ms);
        if (preview)
        {
            cv::imshow("SAB2 video replay", annotated);
            if (cv::waitKey(1) == 27)
                break;
        }
    }
    if (index == 0)
        throw std::runtime_error("No video frames were decoded.");
}
}

int main(int argc, char** argv)
{
    if (argc < 3 || argc > 5)
    {
        std::cerr << "Usage: sab2_video_replay VIDEO ONNX [--coreml] [--preview]\n";
        return 2;
    }
    bool coreml = false;
    bool preview = false;
    for (int i = 3; i < argc; ++i)
    {
        const std::string_view option(argv[i]);
        if (option == "--coreml")
            coreml = true;
        else if (option == "--preview")
            preview = true;
        else
        {
            std::cerr << "Unknown option: " << option << '\n';
            return 2;
        }
    }
    try
    {
        replay(argv[1], argv[2], coreml, preview);
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Replay failed: " << error.what() << '\n';
        return 1;
    }
}
