#pragma once
#include <PlagC/Renderer/Backend.h>
#include <cstdint>
#ifdef PC_ENABLE_PROFILING
#include <memory>
#endif
namespace PlagC
{
struct GpuFrameTiming
{
    bool supported = false;
    bool valid = false;
    double totalMs = 0;
    std::uint32_t latencyFrames = 0;
    // Number of previously submitted frame fences not yet signaled when BeginFrame polls them.
    // This is a non-blocking estimate of GPU work queued ahead, not display scan-out latency.
    std::uint32_t outstandingFrames = 0;
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
};
#endif
} // namespace PlagC
