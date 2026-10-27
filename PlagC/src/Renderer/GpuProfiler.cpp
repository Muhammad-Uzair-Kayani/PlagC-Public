#include "pgcpch.h"
#include <PlagC/GpuProfiler.h>
#include <PlagC/LatencyProfiler.h>
#ifdef PC_ENABLE_PROFILING
#ifndef PC_OGRE_NULL_ONLY
#include <GL/gl3w.h>
#include <OgreRoot.h>
#include <Compositor/OgreCompositorManager2.h>
#include <Compositor/OgreCompositorWorkspace.h>
#include <Compositor/OgreCompositorWorkspaceListener.h>
#include <Compositor/Pass/OgreCompositorPass.h>
#include <Compositor/Pass/OgreCompositorPassDef.h>
#include <Compositor/Pass/PassQuad/OgreCompositorPassQuadDef.h>
#include <Compositor/Pass/PassScene/OgreCompositorPassSceneDef.h>
#endif
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>
namespace PlagC
{
namespace
{
double Average(const std::vector<double>&v){double s=0;for(double x:v)s+=x;return v.empty()?0:s/double(v.size());}
double Maximum(const std::vector<double>&v){return v.empty()?0:*std::max_element(v.begin(),v.end());}
double Percentile(std::vector<double>v,double q){if(v.empty())return 0;std::sort(v.begin(),v.end());auto i=static_cast<std::size_t>(std::ceil(q*double(v.size()))-1);return v[std::min(i,v.size()-1)];}
#ifndef PC_OGRE_NULL_ONLY
GpuPass Classify(const Ogre::CompositorPassDef*def)
{
    if(!def)return GpuPass::Count;
    if(def->getType()==Ogre::PASS_SCENE)
    {
        const auto*s=static_cast<const Ogre::CompositorPassSceneDef*>(def);
        return s->mFirstRQ>=static_cast<Ogre::uint8>(199u)?GpuPass::Overlay:GpuPass::MainScene;
    }
    if(def->getType()!=Ogre::PASS_QUAD)return GpuPass::Count;
    const auto*q=static_cast<const Ogre::CompositorPassQuadDef*>(def);
    const std::string& name=q->mMaterialName;
    if(name=="PlagC/BloomExtract")return GpuPass::BloomExtract;
    if(name=="PlagC/BloomDownsample")return GpuPass::BloomDownsample;
    if(name=="PlagC/BloomBlurH")return GpuPass::BloomBlurH;
    if(name=="PlagC/BloomBlurV")return GpuPass::BloomBlurV;
    if(name=="PlagC/BloomUpsample")return GpuPass::BloomUpsample;
    if(name=="PlagC/ToneMap")return GpuPass::ToneMap;
    if(name.rfind("PlagC/Sky",0)==0)return GpuPass::Sky;
    return GpuPass::Count;
}
class WorkspaceGpuListener final:public Ogre::CompositorWorkspaceListener
{
public:
    void passPreExecute(Ogre::CompositorPass*pass) override
    {
        const auto p=Classify(pass?pass->getDefinition():nullptr);if(p==GpuPass::Count)return;
        if(p==GpuPass::MainScene){scene_=true;shadowEnded_=false;if(pass->getDefinition()){const auto*s=static_cast<const Ogre::CompositorPassSceneDef*>(pass->getDefinition());if(s->mShadowNode!=Ogre::IdString()){GpuProfiler::BeginPass(GpuPass::Shadows);shadow_=true;return;}}}
        GpuProfiler::BeginPass(p);
    }
    void passSceneAfterShadowMaps(Ogre::CompositorPassScene*) override
    {
        if(!scene_||!shadow_)return;GpuProfiler::EndPass(GpuPass::Shadows);shadow_=false;shadowEnded_=true;GpuProfiler::BeginPass(GpuPass::MainScene);
    }
    void passPosExecute(Ogre::CompositorPass*pass) override
    {
        const auto p=Classify(pass?pass->getDefinition():nullptr);if(p==GpuPass::Count)return;
        if(p==GpuPass::MainScene){if(shadow_){GpuProfiler::EndPass(GpuPass::Shadows);shadow_=false;}GpuProfiler::EndPass(GpuPass::MainScene);scene_=shadowEnded_=false;return;}
        GpuProfiler::EndPass(p);
    }
private:bool scene_=false,shadow_=false,shadowEnded_=false;
};
WorkspaceGpuListener workspaceListener;
#endif
}
struct GpuProfiler::Impl
{
    static constexpr std::uint32_t RingSize=8;
    struct PassQuery{
#ifndef PC_OGRE_NULL_ONLY
        GLuint begin=0,end=0;
#endif
        bool begun=false,issued=false;
    };
    struct Slot{
#ifndef PC_OGRE_NULL_ONLY
        GLuint begin=0,end=0;GLsync fence=nullptr;
#endif
        bool pending=false,started=false;std::uint64_t frame=0;std::array<PassQuery,GpuPassCount> passes{};
    };
    RendererBackend backend=RendererBackend::Null;std::array<Slot,RingSize> slots{};std::uint32_t write=0;std::uint64_t frame=0;GpuFrameTiming latest{};std::array<std::vector<double>,GpuPassCount> samples{};
#ifndef PC_OGRE_NULL_ONLY
    Ogre::CompositorManager2* compositorManager=nullptr;
#endif
};
namespace{GpuProfiler*active=nullptr;}
GpuProfiler::GpuProfiler(RendererBackend backend):impl_(std::make_unique<Impl>()){auto&p=*impl_;p.backend=backend;
#ifndef PC_OGRE_NULL_ONLY
if(backend==RendererBackend::OpenGL&&glGenQueries&&glQueryCounter&&glGetQueryObjectiv&&glGetQueryObjectui64v&&glFenceSync&&glClientWaitSync){for(auto&slot:p.slots){glGenQueries(1,&slot.begin);glGenQueries(1,&slot.end);for(auto&q:slot.passes){glGenQueries(1,&q.begin);glGenQueries(1,&q.end);}}p.latest.supported=true;active=this;if(auto*root=Ogre::Root::getSingletonPtr()){p.compositorManager=root->getCompositorManager2();if(p.compositorManager)p.compositorManager->addListener(&workspaceListener);}}
#endif
}
GpuProfiler::~GpuProfiler(){
#ifndef PC_OGRE_NULL_ONLY
if(impl_&&impl_->compositorManager)impl_->compositorManager->removeListener(&workspaceListener);if(active==this)active=nullptr;if(impl_&&impl_->latest.supported&&impl_->backend==RendererBackend::OpenGL)for(auto&slot:impl_->slots){if(slot.fence)glDeleteSync(slot.fence);if(slot.begin)glDeleteQueries(1,&slot.begin);if(slot.end)glDeleteQueries(1,&slot.end);for(auto&q:slot.passes){if(q.begin)glDeleteQueries(1,&q.begin);if(q.end)glDeleteQueries(1,&q.end);}}
#endif
}
void GpuProfiler::AttachWorkspace(void*workspace){
#ifndef PC_OGRE_NULL_ONLY
if(workspace)static_cast<Ogre::CompositorWorkspace*>(workspace)->addListener(&workspaceListener);
#else
(void)workspace;
#endif
}
void GpuProfiler::BeginFrame(){auto&p=*impl_;++p.frame;p.latest.valid=false;p.latest.outstandingFrames=0;
#ifndef PC_OGRE_NULL_ONLY
if(!p.latest.supported||p.backend!=RendererBackend::OpenGL)return;for(auto&slot:p.slots){if(slot.fence){const GLenum status=glClientWaitSync(slot.fence,0,0);if(status==GL_ALREADY_SIGNALED||status==GL_CONDITION_SATISFIED){glDeleteSync(slot.fence);slot.fence=nullptr;}else ++p.latest.outstandingFrames;}if(!slot.pending)continue;GLint ready=GL_FALSE;glGetQueryObjectiv(slot.end,GL_QUERY_RESULT_AVAILABLE,&ready);if(ready!=GL_TRUE)continue;GLuint64 begin=0,end=0;glGetQueryObjectui64v(slot.begin,GL_QUERY_RESULT,&begin);glGetQueryObjectui64v(slot.end,GL_QUERY_RESULT,&end);if(end>=begin){p.latest.valid=true;p.latest.totalMs=double(end-begin)/1000000.0;p.latest.latencyFrames=static_cast<std::uint32_t>(p.frame-slot.frame);for(std::size_t i=0;i<GpuPassCount;++i){auto&q=slot.passes[i];p.latest.passes[i]={};if(!q.issued)continue;GLuint64 qb=0,qe=0;glGetQueryObjectui64v(q.begin,GL_QUERY_RESULT,&qb);glGetQueryObjectui64v(q.end,GL_QUERY_RESULT,&qe);if(qe>=qb){const double value=double(qe-qb)/1000000.0;p.latest.passes[i]={true,value};p.samples[i].push_back(value);}}}slot.pending=false;slot.started=false;for(auto&q:slot.passes)q.begun=q.issued=false;}auto&slot=p.slots[p.write];if(!slot.pending&&!slot.fence){slot.started=true;for(auto&q:slot.passes)q.begun=q.issued=false;glQueryCounter(slot.begin,GL_TIMESTAMP);slot.frame=p.frame;}
#endif
}
void GpuProfiler::EndFrame(){auto&p=*impl_;
#ifndef PC_OGRE_NULL_ONLY
if(!p.latest.supported||p.backend!=RendererBackend::OpenGL)return;auto&slot=p.slots[p.write];if(slot.pending||slot.fence||!slot.started)return;for(auto&q:slot.passes)if(q.begun&&!q.issued){glQueryCounter(q.end,GL_TIMESTAMP);q.issued=true;q.begun=false;}glQueryCounter(slot.end,GL_TIMESTAMP);slot.fence=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);slot.pending=true;slot.started=false;p.write=(p.write+1u)%Impl::RingSize;
#endif
}
void GpuProfiler::BeginPass(GpuPass pass){
#ifndef PC_OGRE_NULL_ONLY
if(!active||pass==GpuPass::Count)return;auto&p=*active->impl_;if(p.backend!=RendererBackend::OpenGL||!p.latest.supported)return;auto&slot=p.slots[p.write];if(!slot.started||slot.pending)return;auto&q=slot.passes[static_cast<std::size_t>(pass)];if(q.begun||q.issued)return;glQueryCounter(q.begin,GL_TIMESTAMP);q.begun=true;
#else
(void)pass;
#endif
}
void GpuProfiler::EndPass(GpuPass pass){
#ifndef PC_OGRE_NULL_ONLY
if(!active||pass==GpuPass::Count)return;auto&p=*active->impl_;if(p.backend!=RendererBackend::OpenGL||!p.latest.supported)return;auto&slot=p.slots[p.write];if(!slot.started||slot.pending)return;auto&q=slot.passes[static_cast<std::size_t>(pass)];if(!q.begun||q.issued)return;glQueryCounter(q.end,GL_TIMESTAMP);q.begun=false;q.issued=true;
#else
(void)pass;
#endif
}
GpuPassStatsArray GpuProfiler::ConsumePassStats(){GpuPassStatsArray out{};for(std::size_t i=0;i<GpuPassCount;++i){auto&v=impl_->samples[i];auto&s=out[i];s.averageMs=Average(v);s.p95Ms=Percentile(v,.95);s.p99Ms=Percentile(v,.99);s.maxMs=Maximum(v);s.samples=v.size();v.clear();}return out;}
GpuFrameTiming GpuProfiler::Latest()const{const auto timing=impl_->latest;LatencyProfiler::OnGpuTiming(timing.supported,timing.valid,timing.totalMs,timing.latencyFrames,timing.outstandingFrames);return timing;}
} // namespace PlagC
#endif
