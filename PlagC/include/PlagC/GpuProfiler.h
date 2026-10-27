#pragma once
#include <PlagC/Renderer/Backend.h>
#include <cstdint>
#include <memory>
namespace PlagC
{
struct GpuFrameTiming
{
    bool supported = false;
    bool valid = false;
    double totalMs = 0;
    std::uint32_t latencyFrames = 0;
};
// Backend-aware, non-blocking GPU timer. BeginFrame/EndFrame must run on the render thread
// with the graphics context current. Results are intentionally consumed several frames late.
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
} // namespace PlagC
