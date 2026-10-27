#include "pgcpch.h"
#include <PlagC/GpuProfiler.h>
#ifdef PC_ENABLE_PROFILING
#ifndef PC_OGRE_NULL_ONLY
#include <GL/gl3w.h>
#endif
#include <array>
namespace PlagC
{
struct GpuProfiler::Impl
{
    static constexpr std::uint32_t RingSize = 4;
    struct Slot
    {
#ifndef PC_OGRE_NULL_ONLY
        GLuint begin = 0, end = 0;
#endif
        bool pending = false;
        std::uint64_t frame = 0;
    };
    RendererBackend backend = RendererBackend::Null;
    std::array<Slot, RingSize> slots{};
    std::uint32_t write = 0;
    std::uint64_t frame = 0;
    GpuFrameTiming latest{};
};
GpuProfiler::GpuProfiler(RendererBackend backend) : impl_(std::make_unique<Impl>())
{
    auto &p = *impl_;
    p.backend = backend;
#ifndef PC_OGRE_NULL_ONLY
    if (backend == RendererBackend::OpenGL)
    {
        if (glGenQueries && glQueryCounter && glGetQueryObjectiv && glGetQueryObjectui64v)
        {
            for (auto &slot : p.slots)
            {
                glGenQueries(1, &slot.begin);
                glGenQueries(1, &slot.end);
            }
            p.latest.supported = true;
        }
    }
#endif
}
GpuProfiler::~GpuProfiler()
{
#ifndef PC_OGRE_NULL_ONLY
    if (impl_ && impl_->latest.supported && impl_->backend == RendererBackend::OpenGL)
        for (auto &slot : impl_->slots)
        {
            if (slot.begin) glDeleteQueries(1, &slot.begin);
            if (slot.end) glDeleteQueries(1, &slot.end);
        }
#endif
}
void GpuProfiler::BeginFrame()
{
    auto &p = *impl_;
    ++p.frame;
    p.latest.valid = false;
#ifndef PC_OGRE_NULL_ONLY
    if (!p.latest.supported || p.backend != RendererBackend::OpenGL) return;
    for (auto &slot : p.slots)
    {
        if (!slot.pending) continue;
        GLint ready = GL_FALSE;
        glGetQueryObjectiv(slot.end, GL_QUERY_RESULT_AVAILABLE, &ready);
        if (ready != GL_TRUE) continue;
        GLuint64 begin = 0, end = 0;
        glGetQueryObjectui64v(slot.begin, GL_QUERY_RESULT, &begin);
        glGetQueryObjectui64v(slot.end, GL_QUERY_RESULT, &end);
        if (end >= begin)
        {
            p.latest.valid = true;
            p.latest.totalMs = double(end - begin) / 1000000.0;
            p.latest.latencyFrames = static_cast<std::uint32_t>(p.frame - slot.frame);
        }
        slot.pending = false;
    }
    auto &slot = p.slots[p.write];
    if (!slot.pending)
    {
        glQueryCounter(slot.begin, GL_TIMESTAMP);
        slot.frame = p.frame;
    }
#endif
}
void GpuProfiler::EndFrame()
{
    auto &p = *impl_;
#ifndef PC_OGRE_NULL_ONLY
    if (!p.latest.supported || p.backend != RendererBackend::OpenGL) return;
    auto &slot = p.slots[p.write];
    if (slot.pending) return;
    glQueryCounter(slot.end, GL_TIMESTAMP);
    slot.pending = true;
    p.write = (p.write + 1u) % Impl::RingSize;
#endif
}
GpuFrameTiming GpuProfiler::Latest() const
{
    return impl_->latest;
}
} // namespace PlagC
#endif // PC_ENABLE_PROFILING
