#include "detector/postProcess.h"
#include "mouse/node_target_settings.h"
#include "nodes/node_graph.h"
#ifdef __APPLE__
#include "portable/screen_capture_kit.h"
#endif

#include <onnxruntime_cxx_api.h>
#include <opencv2/dnn.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
using Clock = std::chrono::steady_clock;

enum class Source { Video, Display, Window };

struct Options
{
    Source source = Source::Video;
    std::string video;
    std::string model;
    std::uint32_t sourceId = 0;
    std::uint32_t maxFrames = 0;
    bool coreml = false;
    bool preview = false;
};

std::uint32_t positiveNumber(std::string_view input, const char* name)
{
    std::uint32_t value = 0;
    const auto end = input.data() + input.size();
    const auto parsed = std::from_chars(input.data(), end, value);
    if (parsed.ec != std::errc{} || parsed.ptr != end || value == 0)
        throw std::invalid_argument(std::string("Expected a positive integer for ") + name);
    return value;
}

Options parseOptions(int argc, char** argv)
{
    Options options;
    int next = 1;
    const std::string_view input(argv[next++]);
    if (input == "--display" || input == "--window")
    {
        if (argc < 4)
            throw std::invalid_argument("Screen capture requires a target ID and ONNX file.");
        options.source = input == "--display" ? Source::Display : Source::Window;
        options.sourceId = positiveNumber(argv[next++], "capture target ID");
        options.maxFrames = 120;
    }
    else
        options.video = input;
    options.model = argv[next++];
    while (next < argc)
    {
        const std::string_view option(argv[next++]);
        if (option == "--coreml")
            options.coreml = true;
        else if (option == "--preview")
            options.preview = true;
        else if (option == "--frames" && next < argc)
            options.maxFrames = positiveNumber(argv[next++], "frame limit");
        else
            throw std::invalid_argument("Invalid replay option: " + std::string(option));
    }
    return options;
}

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

std::optional<LockedTargetInfo> track(const NodeGraph::DetectionFrame& detections, MultiTargetTracker& tracker,
    const TargetSelectionSettings& settings)
{
    std::vector<cv::Rect> boxes;
    std::vector<int> classes;
    std::vector<float> confidences;
    boxes.reserve(detections.objects.size());
    classes.reserve(detections.objects.size());
    confidences.reserve(detections.objects.size());
    for (const auto& detection : detections.objects)
    {
        boxes.emplace_back(static_cast<int>(std::lround(detection.x)), static_cast<int>(std::lround(detection.y)),
            static_cast<int>(std::lround(detection.width)), static_cast<int>(std::lround(detection.height)));
        classes.push_back(detection.classId);
        confidences.push_back(static_cast<float>(detection.confidence));
    }
    // A zero time point asks the tracker for real time. Use a nonzero epoch for replay.
    const auto timestamp = Clock::time_point{} + std::chrono::seconds(1) +
        std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(detections.timestampSeconds));
    tracker.update(boxes, classes, static_cast<int>(detections.width), static_cast<int>(detections.height),
        false, true, settings, confidences, timestamp);
    LockedTargetInfo target;
    if (tracker.getLockedTarget(target))
        return target;
    return std::nullopt;
}

void report(const NodeGraph::DetectionStream& detections, const std::optional<LockedTargetInfo>& target,
    cv::Mat& preview, std::int64_t index, double timestamp, double processingMs)
{
    const auto writeTrack = [&target] {
        if (target)
            std::cout << ',' << target->trackId << ',' << target->observedThisFrame << ','
                << target->target.pivotX << ',' << target->target.pivotY;
        else
            std::cout << ",,,,";
        std::cout << '\n';
    };
    if (detections->objects.empty())
    {
        std::cout << index << ',' << timestamp << ',' << processingMs << ",,,,,,";
        writeTrack();
    }

    for (const auto& detection : detections->objects)
    {
        std::cout << index << ',' << timestamp << ',' << processingMs << ',' << detection.classId
            << ',' << detection.confidence << ',' << detection.x << ',' << detection.y
            << ',' << detection.width << ',' << detection.height;
        writeTrack();
        if (!preview.empty())
        {
            const cv::Rect box(cv::Point(cvRound(detection.x), cvRound(detection.y)),
                cv::Size(cvRound(detection.width), cvRound(detection.height)));
            cv::rectangle(preview, box, cv::Scalar(0, 255, 0), 2);
        }
    }
    if (!preview.empty() && target)
        cv::circle(preview, {cvRound(target->target.pivotX), cvRound(target->target.pivotY)}, 3, {0, 255, 255}, -1);
}

void replay(const Options& options)
{
    cv::VideoCapture video;
    double fps = 0;
    if (options.source == Source::Video)
    {
        if (!video.open(options.video))
            throw std::runtime_error("Cannot open video file: " + options.video);
        fps = video.get(cv::CAP_PROP_FPS);
    }
#ifdef __APPLE__
    std::unique_ptr<Sab2::ScreenCaptureKit> screen;
    std::optional<double> screenEpoch;
    if (options.source != Source::Video)
    {
        const auto target = options.source == Source::Display ? Sab2::ScreenTarget::Display : Sab2::ScreenTarget::Window;
        screen = std::make_unique<Sab2::ScreenCaptureKit>(target, options.sourceId);
        fps = Sab2::ScreenCaptureKit::MaxFps;
    }
#else
    if (options.source != Source::Video)
        throw std::runtime_error("Display and window capture require macOS ScreenCaptureKit.");
#endif
    Model model(options.model, options.coreml);

    NodeGraph::Service graph;
    auto workspace = graph.workspace();
    for (auto& page : workspace.graphs)
        if (page.kind == NodeGraph::GraphKind::Vision)
            for (auto& node : page.nodes)
                if (node.kind == NodeGraph::NodeKind::Inference)
                {
                    node.textParameters["model_path"] = options.model;
                    node.textParameters["backend"] = options.coreml ? "CoreML" : "CPU";
                }
    std::string error;
    if (!graph.publish(workspace, error))
        throw std::runtime_error(error);

    NodeGraph::Context context;
    context.captureFps = std::isfinite(fps) && fps > 0 ? fps : 60;
    context.infer = [&model](int, const auto& packet, const auto& node) { return model.infer(packet, node); };
    MultiTargetTracker tracker;
    const auto selection = nodeTargetSettings(NodeGraph::defaultNode(NodeGraph::NodeKind::TrackDetections, 1));
    std::cout << "frame,seconds,process_ms,class_id,confidence,x,y,width,height,"
        "track_id,track_observed,track_x,track_y\n" << std::setprecision(9);
    std::int64_t index = 0;
    while (options.maxFrames == 0 || index < options.maxFrames)
    {
        cv::Mat frame;
        double seconds = index / context.captureFps;
#ifdef __APPLE__
        if (screen)
        {
            screen->read(frame, seconds);
            if (!screenEpoch)
                screenEpoch = seconds;
            seconds -= *screenEpoch;
        }
        else
#endif
        if (!video.read(frame))
            break;
        const auto started = Clock::now();
        const auto packet = std::make_shared<NodeGraph::FrameData>();
        packet->storage = std::make_shared<cv::Mat>(frame);
        packet->width = frame.cols;
        packet->height = frame.rows;
        packet->version = index;
        packet->generation = 1;
        packet->timestampSeconds = seconds;
        context.elapsedSeconds = packet->timestampSeconds;
        context.center = {frame.cols * 0.5, frame.rows * 0.5};
        context.frame = packet;
        const auto result = graph.evaluateVision(context);
        if (!result.valid || !result.visionDetections)
            throw std::runtime_error("The upstream Vision graph did not publish a detection stream.");
        const auto target = track(*result.visionDetections, tracker, selection);
        const double ms = std::chrono::duration<double, std::milli>(Clock::now() - started).count();
        cv::Mat annotated = options.preview ? frame.clone() : cv::Mat{};
        report(result.visionDetections, target, annotated, index++, packet->timestampSeconds, ms);
        if (options.preview)
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
    try
    {
        if (argc == 2 && std::string_view(argv[1]) == "--list-screens")
        {
#ifdef __APPLE__
            Sab2::ScreenCaptureKit::listTargets(std::cout);
            return 0;
#else
            throw std::runtime_error("Display and window capture require macOS ScreenCaptureKit.");
#endif
        }
        if (argc < 3)
        {
            std::cerr << "Usage: sab2_video_replay VIDEO ONNX [--coreml] [--preview] [--frames N]\n"
                "       sab2_video_replay --list-screens\n"
                "       sab2_video_replay (--display ID | --window ID) ONNX [--coreml] [--preview] [--frames N]\n";
            return 2;
        }
        replay(parseOptions(argc, argv));
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Replay failed: " << error.what() << '\n';
        return 1;
    }
}
