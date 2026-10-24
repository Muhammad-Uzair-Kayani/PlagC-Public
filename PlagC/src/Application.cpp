#include "pgcpch.h"
#include <PlagC/Application.h>
#include <PlagC/Thread.h>
namespace PlagC {
Engine::Engine(const EngineConfig& config) {
    logger_.Write(Severity::Info,"CORE","PlagC Integrated Stress World bootstrap");
#ifdef PC_DIST
    logger_.Write(Severity::Info,"CORE","Configuration: Dist");
#elif defined(NDEBUG)
    logger_.Write(Severity::Info,"CORE","Configuration: Release");
#else
    logger_.Write(Severity::Info,"CORE","Configuration: Debug");
#endif
#ifdef _WIN32
    logger_.Write(Severity::Info,"CORE","Platform: Windows");
#elif defined(__APPLE__)
    logger_.Write(Severity::Info,"CORE","Platform: macOS");
#else
    logger_.Write(Severity::Info,"CORE","Platform: Linux");
#endif
    jobs_ = std::make_unique<JobSystem>(config.workers);
    logger_.Write(Severity::Info,"JOB","Workers: " + std::to_string(jobs_->WorkerCount()));
    if (!config.headless && !config.offscreen) { WindowConfig wc; wc.nullPlatform = config.nullPlatform; window_ = std::make_unique<Window>(events_,wc); }
    RendererConfig rc; rc.nullBackend=config.headless||config.nullPlatform;rc.offscreen=config.offscreen;
    renderer_=std::make_unique<Renderer>(window_.get(),rc);
    SetFPSLocked(true);
    resize_=events_.Subscribe(EventType::FramebufferResize,[this](const Event& e){renderer_->Resize(static_cast<unsigned>(e.x),static_cast<unsigned>(e.y));});
}
void Engine::SetFPSLocked(bool enabled) {
    Thread::RequireMain();
    // Keep the explicit cap independent of monitor refresh. VSync can be opted into
    // separately by clients; unlock always removes it as a second possible cap.
    renderer_->SetVSync(false);
    limiter_.SetLocked(enabled);
}
bool Engine::IsFPSLocked() const { Thread::RequireMain(); return limiter_.IsLocked(); }
void Engine::PaceFrame(FrameLimiter::Clock::time_point start) const {
    Thread::RequireMain();
    const auto delay = limiter_.Remaining(start, FrameLimiter::Clock::now());
    if (delay > FrameLimiter::Clock::duration::zero()) std::this_thread::sleep_for(delay);
}
Engine::~Engine() { jobs_.reset(); resize_.Reset(); renderer_.reset(); window_.reset(); logger_.Write(Severity::Info,"CORE","Shutdown complete"); }
}

