#pragma once

#include <opencv2/core/mat.hpp>

#include <cstdint>
#include <iosfwd>
#include <memory>

namespace Sab2
{
enum class ScreenTarget { Display, Window };

class ScreenCaptureKit
{
public:
    static constexpr int MaxFps = 60;
    static void listTargets(std::ostream& output);

    ScreenCaptureKit(ScreenTarget target, std::uint32_t id);
    ~ScreenCaptureKit();

    void read(cv::Mat& image, double& seconds);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
