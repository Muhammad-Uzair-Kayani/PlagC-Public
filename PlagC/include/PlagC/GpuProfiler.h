#pragma once
#include <PlagC/Renderer/Backend.h>
#include <array>
#include <cstdint>
#ifdef PC_ENABLE_PROFILING
#include <memory>
#endif
namespace PlagC
{
enum class GpuPass : std::uint8_t
{
    Shadows,
    MainScene,
    Sky,
    BloomExtract,
    BloomDownsample,
    BloomBlurH,
    BloomBlurV,
    BloomUpsample,
    ToneMap,
    Overlay,
    Count
};
inline constexpr std::size_t GpuPassCount=static_cast<std::size_t>(GpuPass::Count);
struct GpuPassTiming
{
    bool valid=false;
    double ms=0;
};
struct GpuFrameTiming
{
    bool supported = false;
    bool valid = false;
    double totalMs = 0;
    std::uint32_t latencyFrames = 0;
    // Number of previously submitted frame fences not yet signaled when BeginFrame polls them.
    // This is a non-blocking estimate of GPU work queued ahead, not display scan-out latency.
    std::uint32_t outstandingFrames = 0;
    std::array<GpuPassTiming,GpuPassCount> passes{};
};
#ifdef PC_ENABLE_PROFILING
class GpuProfiler
{
  public:
    explicit GpuProfiler(RendererBackend backend);
    ~GpuProfiler();
    GpuProfiler(const GpuProfiler &) = delete;
    GpuProfiler &operator=(const GpuProfiler &) = delete;
    void BeginFrame();
    void EndFrame();
    GpuFrameTiming Latest() const;
    // Ogre compositor listener hooks. They only emit timestamp commands; they never wait.
    static void BeginPass(GpuPass pass);
    static void EndPass(GpuPass pass);
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
#else
class GpuProfiler
{
  public:
    explicit GpuProfiler(RendererBackend) noexcept {}
    void BeginFrame() noexcept {}
    void EndFrame() noexcept {}
    GpuFrameTiming Latest() const noexcept { return {}; }
    static void BeginPass(GpuPass) noexcept {}
    static void EndPass(GpuPass) noexcept {}
};
#endif
} // namespace PlagC
