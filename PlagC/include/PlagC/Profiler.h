#pragma once
#include <PlagC/Renderer/Renderer.h>
#include <algorithm>
#include <cstdint>

namespace PlagC
{
struct FrameProfile
{
    double frameMs = 0;
    double pollEventsMs = 0;
    double assetPumpMs = 0;
    double gameUpdateMs = 0;
    double physicsMs = 0;
    double animationMs = 0;
    double sceneSyncMs = 0;
    double landscapeMs = 0;
    double audioMs = 0;
    double hudMs = 0;
    double gameOtherMs = 0;
    double renderSubmitMs = 0;
    double rendererCpuMs = 0;
    double pacingMs = 0;
    double unaccountedMs = 0;
    double fps = 0;
    std::uint64_t samples = 0;
    RenderStats render{};
};

// Low-overhead owner-thread profiler. Frame samples are accumulated and published
// once per second. Latest() is intentionally UI-friendly so the future ImGui
// profiler/console can consume the same data without changing instrumentation.
class Profiler
{
  public:
    void Submit(const FrameProfile &sample)
    {
        ++count_;
        sum_.frameMs += sample.frameMs;
        sum_.pollEventsMs += sample.pollEventsMs;
        sum_.assetPumpMs += sample.assetPumpMs;
        sum_.gameUpdateMs += sample.gameUpdateMs;
        sum_.physicsMs += sample.physicsMs;
        sum_.animationMs += sample.animationMs;
        sum_.sceneSyncMs += sample.sceneSyncMs;
        sum_.landscapeMs += sample.landscapeMs;
        sum_.audioMs += sample.audioMs;
        sum_.hudMs += sample.hudMs;
        sum_.gameOtherMs += sample.gameOtherMs;
        sum_.renderSubmitMs += sample.renderSubmitMs;
        sum_.rendererCpuMs += sample.rendererCpuMs;
        sum_.pacingMs += sample.pacingMs;
        sum_.unaccountedMs += sample.unaccountedMs;
        elapsedMs_ += sample.frameMs;
        lastRender_ = sample.render;
        if (elapsedMs_ < 1000.0)
            return;

        const double n = double(count_);
        latest_ = sum_;
        latest_.frameMs /= n;
        latest_.pollEventsMs /= n;
        latest_.assetPumpMs /= n;
        latest_.gameUpdateMs /= n;
        latest_.physicsMs /= n;
        latest_.animationMs /= n;
        latest_.sceneSyncMs /= n;
        latest_.landscapeMs /= n;
        latest_.audioMs /= n;
        latest_.hudMs /= n;
        latest_.gameOtherMs /= n;
        latest_.renderSubmitMs /= n;
        latest_.rendererCpuMs /= n;
        latest_.pacingMs /= n;
        latest_.unaccountedMs /= n;
        latest_.fps = elapsedMs_ > 0 ? double(count_) * 1000.0 / elapsedMs_ : 0;
        latest_.samples = count_;
        latest_.render = lastRender_;
        ready_ = true;
        sum_ = {};
        count_ = 0;
        elapsedMs_ = 0;
    }

    bool ConsumePublished()
    {
        const bool value = ready_;
        ready_ = false;
        return value;
    }
    const FrameProfile &Latest() const { return latest_; }

  private:
    FrameProfile sum_{};
    FrameProfile latest_{};
    RenderStats lastRender_{};
    std::uint64_t count_ = 0;
    double elapsedMs_ = 0;
    bool ready_ = false;
};
} // namespace PlagC
