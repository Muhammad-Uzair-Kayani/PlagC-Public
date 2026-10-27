#pragma once
#include <PlagC/Renderer/Renderer.h>
#include <cstdint>

namespace PlagC
{
struct AnimationProfile
{
    double stateMachineMs = 0, samplingMs = 0, localToModelMs = 0, skinningMs = 0, uploadMs = 0,
           otherMs = 0;
};
struct RendererProfile
{
    double scenePreparationMs = 0, shadowMs = 0, geometryMs = 0, lightingMs = 0,
           skyEnvironmentMs = 0, postProcessingMs = 0, presentWaitMs = 0, otherMs = 0;
    bool gpuTimingsValid = false;
};
struct FrameProfile
{
    double frameMs = 0, pollEventsMs = 0, assetPumpMs = 0, gameUpdateMs = 0;
    double physicsMs = 0, animationMs = 0, sceneSyncMs = 0, landscapeMs = 0, audioMs = 0,
           hudMs = 0, gameOtherMs = 0;
    double renderSubmitMs = 0, rendererCpuMs = 0, pacingMs = 0, unaccountedMs = 0, fps = 0;
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
        PC_ACCUM(frameMs); PC_ACCUM(pollEventsMs); PC_ACCUM(assetPumpMs); PC_ACCUM(gameUpdateMs);
        PC_ACCUM(physicsMs); PC_ACCUM(animationMs); PC_ACCUM(sceneSyncMs); PC_ACCUM(landscapeMs);
        PC_ACCUM(audioMs); PC_ACCUM(hudMs); PC_ACCUM(gameOtherMs); PC_ACCUM(renderSubmitMs);
        PC_ACCUM(rendererCpuMs); PC_ACCUM(pacingMs); PC_ACCUM(unaccountedMs);
        PC_ACCUM(animation.stateMachineMs); PC_ACCUM(animation.samplingMs);
        PC_ACCUM(animation.localToModelMs); PC_ACCUM(animation.skinningMs); PC_ACCUM(animation.uploadMs);
        PC_ACCUM(animation.otherMs);
        PC_ACCUM(renderer.scenePreparationMs); PC_ACCUM(renderer.shadowMs); PC_ACCUM(renderer.geometryMs);
        PC_ACCUM(renderer.lightingMs); PC_ACCUM(renderer.skyEnvironmentMs);
        PC_ACCUM(renderer.postProcessingMs); PC_ACCUM(renderer.presentWaitMs); PC_ACCUM(renderer.otherMs);
#undef PC_ACCUM
        elapsedMs_ += s.frameMs;
        lastRender_ = s.render;
        rendererGpuValid_ = rendererGpuValid_ || s.renderer.gpuTimingsValid;
        if (elapsedMs_ < 1000.0) return;
        const double n = double(count_);
#define PC_AVG(x) latest_.x = sum_.x / n
        PC_AVG(frameMs); PC_AVG(pollEventsMs); PC_AVG(assetPumpMs); PC_AVG(gameUpdateMs);
        PC_AVG(physicsMs); PC_AVG(animationMs); PC_AVG(sceneSyncMs); PC_AVG(landscapeMs);
        PC_AVG(audioMs); PC_AVG(hudMs); PC_AVG(gameOtherMs); PC_AVG(renderSubmitMs);
        PC_AVG(rendererCpuMs); PC_AVG(pacingMs); PC_AVG(unaccountedMs);
        PC_AVG(animation.stateMachineMs); PC_AVG(animation.samplingMs);
        PC_AVG(animation.localToModelMs); PC_AVG(animation.skinningMs); PC_AVG(animation.uploadMs);
        PC_AVG(animation.otherMs);
        PC_AVG(renderer.scenePreparationMs); PC_AVG(renderer.shadowMs); PC_AVG(renderer.geometryMs);
        PC_AVG(renderer.lightingMs); PC_AVG(renderer.skyEnvironmentMs);
        PC_AVG(renderer.postProcessingMs); PC_AVG(renderer.presentWaitMs); PC_AVG(renderer.otherMs);
#undef PC_AVG
        latest_.fps = elapsedMs_ > 0 ? double(count_) * 1000.0 / elapsedMs_ : 0;
        latest_.samples = count_; latest_.render = lastRender_;
        latest_.renderer.gpuTimingsValid = rendererGpuValid_;
        ready_ = true; sum_ = {}; count_ = 0; elapsedMs_ = 0; rendererGpuValid_ = false;
    }
    bool ConsumePublished() { const bool v = ready_; ready_ = false; return v; }
    const FrameProfile &Latest() const { return latest_; }
  private:
    FrameProfile sum_{}, latest_{}; RenderStats lastRender_{};
    std::uint64_t count_ = 0; double elapsedMs_ = 0; bool ready_ = false, rendererGpuValid_ = false;
};
} // namespace PlagC
