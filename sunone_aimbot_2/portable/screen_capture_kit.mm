#include "portable/screen_capture_kit.h"

#import <CoreFoundation/CoreFoundation.h>
#import <CoreGraphics/CoreGraphics.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <Foundation/Foundation.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#import <dispatch/dispatch.h>
#include <opencv2/imgproc.hpp>

#include <chrono>
#include <cmath>
#include <condition_variable>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
const char* text(NSString* value)
{
    const char* chars = value.UTF8String;
    return chars ? chars : "";
}

bool waitForCallback(dispatch_semaphore_t ready, std::chrono::seconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    do
    {
        if (dispatch_semaphore_wait(ready, DISPATCH_TIME_NOW) == 0)
            return true;
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.01, false);
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

void await(dispatch_semaphore_t ready, const char* operation)
{
    if (!waitForCallback(ready, std::chrono::seconds(5)))
        throw std::runtime_error(std::string("ScreenCaptureKit timed out: ") + operation);
}

SCShareableContent* shareableContent()
{
    if (!CGPreflightScreenCaptureAccess())
        throw std::runtime_error("Screen Recording permission is required. Grant it to the terminal or application and relaunch.");

    auto ready = dispatch_semaphore_create(0);
    __block SCShareableContent* content = nil;
    __block NSError* failure = nil;
    [SCShareableContent getShareableContentExcludingDesktopWindows:YES onScreenWindowsOnly:YES
        completionHandler:^(SCShareableContent* result, NSError* error) {
            content = result;
            failure = error;
            dispatch_semaphore_signal(ready);
        }];
    await(ready, "list available targets");
    if (!content)
        throw std::runtime_error(std::string("Cannot list capture targets: ") + text(failure.localizedDescription));
    return content;
}

SCContentFilter* contentFilter(Sab2::ScreenTarget target, std::uint32_t id)
{
    const auto content = shareableContent();
    if (target == Sab2::ScreenTarget::Display)
    {
        for (SCDisplay* display in content.displays)
            if (display.displayID == id)
                return [[SCContentFilter alloc] initWithDisplay:display excludingWindows:@[]];
    }
    else
    {
        for (SCWindow* window in content.windows)
            if (window.windowID == id)
                return [[SCContentFilter alloc] initWithDesktopIndependentWindow:window];
    }
    throw std::runtime_error("Capture target is unavailable. Check its current ID with --list-screens.");
}
}

@interface SAB2CaptureOutput : NSObject <SCStreamOutput, SCStreamDelegate>
{
@public
    std::mutex mutex;
    std::condition_variable ready;
    cv::Mat image;
    double timestamp;
    std::string failure;
}
@end

@implementation SAB2CaptureOutput
- (void)stream:(SCStream*)stream didOutputSampleBuffer:(CMSampleBufferRef)sample ofType:(SCStreamOutputType)type
{
    if (type != SCStreamOutputTypeScreen || !CMSampleBufferDataIsReady(sample))
        return;
    NSArray<NSDictionary*>* attachments = (__bridge NSArray*)CMSampleBufferGetSampleAttachmentsArray(sample, false);
    NSNumber* status = attachments.firstObject[SCStreamFrameInfoStatus];
    if (!status || status.integerValue != SCFrameStatusComplete)
        return;

    const double seconds = CMTimeGetSeconds(CMSampleBufferGetPresentationTimeStamp(sample));
    CVPixelBufferRef buffer = CMSampleBufferGetImageBuffer(sample);
    if (!std::isfinite(seconds) || !buffer || CVPixelBufferGetPixelFormatType(buffer) != kCVPixelFormatType_32BGRA ||
        CVPixelBufferLockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly) != kCVReturnSuccess)
        return;

    cv::Mat converted;
    std::string error;
    try
    {
        const cv::Mat pixels(static_cast<int>(CVPixelBufferGetHeight(buffer)),
            static_cast<int>(CVPixelBufferGetWidth(buffer)), CV_8UC4,
            CVPixelBufferGetBaseAddress(buffer), CVPixelBufferGetBytesPerRow(buffer));
        cv::cvtColor(pixels, converted, cv::COLOR_BGRA2BGR);
    }
    catch (const std::exception& cause)
    {
        error = cause.what();
    }
    CVPixelBufferUnlockBaseAddress(buffer, kCVPixelBufferLock_ReadOnly);
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (!error.empty())
            failure = std::move(error);
        else
        {
            image = std::move(converted);
            timestamp = seconds;
        }
    }
    ready.notify_one();
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error
{
    {
        std::lock_guard<std::mutex> lock(mutex);
        failure = std::string("Screen capture stopped: ") + text(error.localizedDescription);
    }
    ready.notify_one();
}
@end

namespace Sab2
{
struct ScreenCaptureKit::Impl
{
    SAB2CaptureOutput* output = [[SAB2CaptureOutput alloc] init];
    SCStream* stream = nil;
    dispatch_queue_t queue = dispatch_queue_create("sab2.screen.capture", DISPATCH_QUEUE_SERIAL);
    bool started = false;

    ~Impl()
    {
        if (!started)
            return;
        auto done = dispatch_semaphore_create(0);
        [stream stopCaptureWithCompletionHandler:^(NSError*) { dispatch_semaphore_signal(done); }];
        waitForCallback(done, std::chrono::seconds(2));
    }
};

void ScreenCaptureKit::listTargets(std::ostream& output)
{
    @autoreleasepool
    {
        const auto content = shareableContent();
        for (SCDisplay* display in content.displays)
            output << "display\t" << display.displayID << '\t' << display.width << 'x' << display.height << '\n';
        for (SCWindow* window in content.windows)
            output << "window\t" << window.windowID << '\t' << text(window.owningApplication.applicationName)
                << '\t' << text(window.title) << '\n';
    }
}

ScreenCaptureKit::ScreenCaptureKit(ScreenTarget target, std::uint32_t id)
    : impl_(std::make_unique<Impl>())
{
    @autoreleasepool
    {
        const auto filter = contentFilter(target, id);
        const auto configuration = [[SCStreamConfiguration alloc] init];
        if (@available(macOS 14.0, *))
        {
            configuration.width = std::lround(CGRectGetWidth(filter.contentRect) * filter.pointPixelScale);
            configuration.height = std::lround(CGRectGetHeight(filter.contentRect) * filter.pointPixelScale);
        }
        configuration.minimumFrameInterval = CMTimeMake(1, MaxFps);
        configuration.pixelFormat = kCVPixelFormatType_32BGRA;
        configuration.queueDepth = 3;
        configuration.showsCursor = NO;
        configuration.capturesAudio = NO;

        impl_->stream = [[SCStream alloc] initWithFilter:filter configuration:configuration delegate:impl_->output];
        NSError* error = nil;
        if (![impl_->stream addStreamOutput:impl_->output type:SCStreamOutputTypeScreen
                sampleHandlerQueue:impl_->queue error:&error])
            throw std::runtime_error(std::string("Cannot configure screen capture: ") + text(error.localizedDescription));

        auto done = dispatch_semaphore_create(0);
        __block NSError* failure = nil;
        [impl_->stream startCaptureWithCompletionHandler:^(NSError* cause) {
            failure = cause;
            dispatch_semaphore_signal(done);
        }];
        impl_->started = true;
        await(done, "start capture");
        if (failure)
        {
            impl_->started = false;
            throw std::runtime_error(std::string("Cannot start screen capture: ") + text(failure.localizedDescription));
        }
    }
}

ScreenCaptureKit::~ScreenCaptureKit() = default;

void ScreenCaptureKit::read(cv::Mat& image, double& seconds)
{
    auto* output = impl_->output;
    std::unique_lock<std::mutex> lock(output->mutex);
    if (!output->ready.wait_for(lock, std::chrono::seconds(5),
            [output] { return !output->image.empty() || !output->failure.empty(); }))
        throw std::runtime_error("No complete screen frame arrived within five seconds.");
    if (!output->failure.empty())
        throw std::runtime_error(output->failure);
    image = std::move(output->image);
    seconds = output->timestamp;
}
}
