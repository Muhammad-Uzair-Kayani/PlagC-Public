#pragma once

#include <PlagC/Renderer/Renderer.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace PlagC
{
struct AnimationProfile
{
    double poseMs = 0.0, skinningMs = 0.0, uploadMs = 0.0, otherMs = 0.0;
};

struct RendererProfile
{
    // CPU-side boundaries. renderFrameMs is Ogre::renderOneFrame wall time and may
    // include driver/GPU waits.
    double animationTickMs = 0.0, renderFrameMs = 0.0, metricsMs = 0.0, otherMs = 0.0;
};

struct DetailedFrameTimings
{
    AnimationProfile animation{};
    RendererProfile renderer{};
};

inline DetailedFrameTimings &DetailedTimings()
{
    static thread_local DetailedFrameTimings timings;
    return timings;
}

struct FrameProfile
{
    double frameMs = 0.0, pollEventsMs = 0.0, assetPumpMs = 0.0, applicationUpdateMs = 0.0;
    double physicsMs = 0.0, animationMs = 0.0, sceneSyncMs = 0.0, audioMs = 0.0,
           applicationOtherMs = 0.0;
    double renderSubmitMs = 0.0, rendererCpuMs = 0.0, pacingMs = 0.0, unaccountedMs = 0.0,
           fps = 0.0;

    double frameP95Ms = 0.0, frameP99Ms = 0.0, frameMaxMs = 0.0;
    double eventsP99Ms = 0.0, eventsMaxMs = 0.0;
    double applicationP99Ms = 0.0, applicationMaxMs = 0.0;
    double renderP99Ms = 0.0, renderMaxMs = 0.0;

    bool gpuSupported = false, gpuValid = false;
    double gpuTotalMs = 0.0, gpuP99Ms = 0.0, gpuMaxMs = 0.0;
    std::uint32_t gpuLatencyFrames = 0;
    std::uint64_t gpuSamples = 0;
    std::uint64_t hitch16Count = 0, hitch33Count = 0;
    std::uint64_t samples = 0;

    AnimationProfile animation{};
    RendererProfile renderer{};
    RenderStats render{};
};

class Profiler
{
  public:
    void Submit(const FrameProfile &s)
    {
        ++count_;
#define PC_ACCUM(x) sum_.x += s.x
        PC_ACCUM(frameMs);
        PC_ACCUM(pollEventsMs);
        PC_ACCUM(assetPumpMs);
        PC_ACCUM(applicationUpdateMs);
        PC_ACCUM(physicsMs);
        PC_ACCUM(animationMs);
        PC_ACCUM(sceneSyncMs);
        PC_ACCUM(audioMs);
        PC_ACCUM(applicationOtherMs);
        PC_ACCUM(renderSubmitMs);
        PC_ACCUM(rendererCpuMs);
        PC_ACCUM(pacingMs);
        PC_ACCUM(unaccountedMs);
        PC_ACCUM(animation.poseMs);
        PC_ACCUM(animation.skinningMs);
        PC_ACCUM(animation.uploadMs);
        PC_ACCUM(animation.otherMs);
        PC_ACCUM(renderer.animationTickMs);
        PC_ACCUM(renderer.renderFrameMs);
        PC_ACCUM(renderer.metricsMs);
        PC_ACCUM(renderer.otherMs);
#undef PC_ACCUM

        frameSamples_.push_back(s.frameMs);
        eventSamples_.push_back(s.pollEventsMs);
        applicationSamples_.push_back(s.applicationUpdateMs);
        renderSamples_.push_back(s.renderSubmitMs);

        gpuSupported_ = gpuSupported_ || s.gpuSupported;
        if (s.gpuValid)
        {
            gpuSumMs_ += s.gpuTotalMs;
            gpuSamples_.push_back(s.gpuTotalMs);
            gpuLatencyFrames_ = s.gpuLatencyFrames;
        }

        if (s.frameMs > 16.667)
            ++hitch16Count_;
        if (s.frameMs > 33.333)
            ++hitch33Count_;

        elapsedMs_ += s.frameMs;
        lastRender_ = s.render;
        if (elapsedMs_ < 1000.0)
            return;

        const double n = double(count_);
#define PC_AVG(x) latest_.x = sum_.x / n
        PC_AVG(frameMs);
        PC_AVG(pollEventsMs);
        PC_AVG(assetPumpMs);
        PC_AVG(applicationUpdateMs);
        PC_AVG(physicsMs);
        PC_AVG(animationMs);
        PC_AVG(sceneSyncMs);
        PC_AVG(audioMs);
        PC_AVG(applicationOtherMs);
        PC_AVG(renderSubmitMs);
        PC_AVG(rendererCpuMs);
        PC_AVG(pacingMs);
        PC_AVG(unaccountedMs);
        PC_AVG(animation.poseMs);
        PC_AVG(animation.skinningMs);
        PC_AVG(animation.uploadMs);
        PC_AVG(animation.otherMs);
        PC_AVG(renderer.animationTickMs);
        PC_AVG(renderer.renderFrameMs);
        PC_AVG(renderer.metricsMs);
        PC_AVG(renderer.otherMs);
#undef PC_AVG

        auto percentile = [](std::vector<double> values, double q) {
            if (values.empty())
                return 0.0;
            std::sort(values.begin(), values.end());
            const auto index =
                static_cast<std::size_t>(std::ceil(q * double(values.size())) - 1.0);
            return values[std::min(index, values.size() - 1)];
        };
        auto maximum = [](const std::vector<double> &values) {
            return values.empty() ? 0.0 : *std::max_element(values.begin(), values.end());
        };

        latest_.frameP95Ms = percentile(frameSamples_, .95);
        latest_.frameP99Ms = percentile(frameSamples_, .99);
        latest_.frameMaxMs = maximum(frameSamples_);
        latest_.eventsP99Ms = percentile(eventSamples_, .99);
        latest_.eventsMaxMs = maximum(eventSamples_);
        latest_.applicationP99Ms = percentile(applicationSamples_, .99);
        latest_.applicationMaxMs = maximum(applicationSamples_);
        latest_.renderP99Ms = percentile(renderSamples_, .99);
        latest_.renderMaxMs = maximum(renderSamples_);

        latest_.gpuSupported = gpuSupported_;
        latest_.gpuValid = !gpuSamples_.empty();
        latest_.gpuSamples = gpuSamples_.size();
        latest_.gpuLatencyFrames = gpuLatencyFrames_;
        latest_.gpuTotalMs =
            gpuSamples_.empty() ? 0.0 : gpuSumMs_ / double(gpuSamples_.size());
        latest_.gpuP99Ms = percentile(gpuSamples_, .99);
        latest_.gpuMaxMs = maximum(gpuSamples_);

        latest_.hitch16Count = hitch16Count_;
        latest_.hitch33Count = hitch33Count_;
        latest_.fps = elapsedMs_ > 0.0 ? double(count_) * 1000.0 / elapsedMs_ : 0.0;
        latest_.samples = count_;
        latest_.render = lastRender_;

        ready_ = true;
        sum_ = {};
        count_ = 0;
        elapsedMs_ = 0.0;
        frameSamples_.clear();
        eventSamples_.clear();
        applicationSamples_.clear();
        renderSamples_.clear();
        gpuSamples_.clear();
        gpuSumMs_ = 0.0;
        gpuSupported_ = false;
        gpuLatencyFrames_ = 0;
        hitch16Count_ = hitch33Count_ = 0;
    }

    bool ConsumePublished()
    {
        const bool value = ready_;
        ready_ = false;
        return value;
    }

    const FrameProfile &Latest() const { return latest_; }

  private:
    FrameProfile sum_{}, latest_{};
    RenderStats lastRender_{};
    std::vector<double> frameSamples_, eventSamples_, applicationSamples_, renderSamples_,
        gpuSamples_;
    std::uint64_t count_ = 0, hitch16Count_ = 0, hitch33Count_ = 0;
    double elapsedMs_ = 0.0, gpuSumMs_ = 0.0;
    bool ready_ = false, gpuSupported_ = false;
    std::uint32_t gpuLatencyFrames_ = 0;
};
} // namespace PlagC
