#include "pgcpch.h"
#include <PlagC/GpuProfiler.h>
#include <PlagC/LatencyProfiler.h>
#ifdef PC_ENABLE_PROFILING
#ifndef PC_OGRE_NULL_ONLY
#include <GL/gl3w.h>
#endif
#include <array>
namespace PlagC
{
struct GpuProfiler::Impl{static constexpr std::uint32_t RingSize=8;struct Slot{
#ifndef PC_OGRE_NULL_ONLY
GLuint begin=0,end=0;GLsync fence=nullptr;
#endif
bool pending=false;std::uint64_t frame=0;};RendererBackend backend=RendererBackend::Null;std::array<Slot,RingSize> slots{};std::uint32_t write=0;std::uint64_t frame=0;GpuFrameTiming latest{};};
GpuProfiler::GpuProfiler(RendererBackend backend):impl_(std::make_unique<Impl>()){auto&p=*impl_;p.backend=backend;
#ifndef PC_OGRE_NULL_ONLY
if(backend==RendererBackend::OpenGL&&glGenQueries&&glQueryCounter&&glGetQueryObjectiv&&glGetQueryObjectui64v&&glFenceSync&&glClientWaitSync){for(auto&slot:p.slots){glGenQueries(1,&slot.begin);glGenQueries(1,&slot.end);}p.latest.supported=true;}
#endif
}
GpuProfiler::~GpuProfiler(){
#ifndef PC_OGRE_NULL_ONLY
if(impl_&&impl_->latest.supported&&impl_->backend==RendererBackend::OpenGL)for(auto&slot:impl_->slots){if(slot.fence)glDeleteSync(slot.fence);if(slot.begin)glDeleteQueries(1,&slot.begin);if(slot.end)glDeleteQueries(1,&slot.end);}
#endif
}
void GpuProfiler::BeginFrame(){auto&p=*impl_;++p.frame;p.latest.valid=false;p.latest.outstandingFrames=0;
#ifndef PC_OGRE_NULL_ONLY
if(!p.latest.supported||p.backend!=RendererBackend::OpenGL)return;for(auto&slot:p.slots){if(slot.fence){const GLenum status=glClientWaitSync(slot.fence,0,0);if(status==GL_ALREADY_SIGNALED||status==GL_CONDITION_SATISFIED){glDeleteSync(slot.fence);slot.fence=nullptr;}else ++p.latest.outstandingFrames;}if(!slot.pending)continue;GLint ready=GL_FALSE;glGetQueryObjectiv(slot.end,GL_QUERY_RESULT_AVAILABLE,&ready);if(ready!=GL_TRUE)continue;GLuint64 begin=0,end=0;glGetQueryObjectui64v(slot.begin,GL_QUERY_RESULT,&begin);glGetQueryObjectui64v(slot.end,GL_QUERY_RESULT,&end);if(end>=begin){p.latest.valid=true;p.latest.totalMs=double(end-begin)/1000000.0;p.latest.latencyFrames=static_cast<std::uint32_t>(p.frame-slot.frame);}slot.pending=false;}auto&slot=p.slots[p.write];if(!slot.pending&&!slot.fence){glQueryCounter(slot.begin,GL_TIMESTAMP);slot.frame=p.frame;}
#endif
}
void GpuProfiler::EndFrame(){auto&p=*impl_;
#ifndef PC_OGRE_NULL_ONLY
if(!p.latest.supported||p.backend!=RendererBackend::OpenGL)return;auto&slot=p.slots[p.write];if(slot.pending||slot.fence)return;glQueryCounter(slot.end,GL_TIMESTAMP);slot.fence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);slot.pending=true;p.write=(p.write+1u)%Impl::RingSize;
#endif
}
GpuFrameTiming GpuProfiler::Latest() const{const auto timing=impl_->latest;LatencyProfiler::OnGpuTiming(timing.supported,timing.valid,timing.totalMs,timing.latencyFrames,timing.outstandingFrames);return timing;}
} // namespace PlagC
#endif
