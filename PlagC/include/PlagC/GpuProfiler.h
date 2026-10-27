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
struct GpuPassTiming{bool valid=false;double ms=0;};
struct GpuPassStats{double averageMs=0,p95Ms=0,p99Ms=0,maxMs=0;std::uint64_t samples=0;};
using GpuPassStatsArray=std::array<GpuPassStats,GpuPassCount>;
struct GpuFrameTiming
{
    bool supported=false,valid=false;double totalMs=0;std::uint32_t latencyFrames=0;
    // Non-blocking estimate of submitted GPU frames still in flight; not display latency.
    std::uint32_t outstandingFrames=0;
    std::array<GpuPassTiming,GpuPassCount> passes{};
};
#ifdef PC_ENABLE_PROFILING
class GpuProfiler
{
  public:
    explicit GpuProfiler(RendererBackend backend);~GpuProfiler();
    GpuProfiler(const GpuProfiler&)=delete;GpuProfiler&operator=(const GpuProfiler&)=delete;
    void BeginFrame();void EndFrame();GpuFrameTiming Latest() const;
    GpuPassStatsArray ConsumePassStats();
    // Renderer bridge. Pass an Ogre::CompositorWorkspace*. Kept opaque so Ogre stays out of public headers.
    static void AttachWorkspace(void* workspace);
    static void BeginPass(GpuPass pass);static void EndPass(GpuPass pass);
  private:struct Impl;std::unique_ptr<Impl> impl_;
};
#else
class GpuProfiler
{
  public:
    explicit GpuProfiler(RendererBackend) noexcept{}void BeginFrame() noexcept{}void EndFrame() noexcept{}
    GpuFrameTiming Latest() const noexcept{return {};}GpuPassStatsArray ConsumePassStats() noexcept{return {};}
    static void AttachWorkspace(void*) noexcept{}static void BeginPass(GpuPass) noexcept{}static void EndPass(GpuPass) noexcept{}
};
#endif
} // namespace PlagC
