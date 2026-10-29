#include "pgcpch.h"
#include <PlagC/Application.h>
#include <PlagC/Renderer/BackendSelection.h>
#include <PlagC/Thread.h>
#include <iostream>

namespace PlagC
{
Engine::Engine(const EngineConfig &config)
{
    logger_.Write(Severity::Info, "CORE", "PlagC engine bootstrap");
#ifdef PC_DIST
    logger_.Write(Severity::Info, "CORE", "Configuration: Dist");
#elif defined(NDEBUG)
    logger_.Write(Severity::Info, "CORE", "Configuration: Release");
#else
    logger_.Write(Severity::Info, "CORE", "Configuration: Debug");
#endif
#ifdef _WIN32
    logger_.Write(Severity::Info, "CORE", "Platform: Windows");
#elif defined(__APPLE__)
    logger_.Write(Severity::Info, "CORE", "Platform: macOS");
#else
    logger_.Write(Severity::Info, "CORE", "Platform: Linux");
#endif

    const auto availableBackends = Renderer::BackendCapabilities();
    RendererBackend requestedBackend = config.rendererBackend;
    bool interactiveRendererSelection =
        config.rendererSelectionPrompt && requestedBackend == RendererBackend::Auto &&
        !config.headless && !config.nullPlatform && !config.offscreen;
#ifdef PC_DIST
    interactiveRendererSelection = false;
#endif
    if (interactiveRendererSelection)
        requestedBackend = PromptRendererBackend(availableBackends, std::cin, std::cout);

    const auto backend =
        ResolveRendererBackend(requestedBackend, availableBackends,
                               config.headless || config.nullPlatform, config.offscreen);

    jobs_ = std::make_unique<JobSystem>(config.workers);
    logger_.Write(Severity::Info, "JOB",
                  "Workers: " + std::to_string(jobs_->WorkerCount()));

    if (!config.headless && !config.offscreen)
    {
        WindowConfig windowConfig;
        windowConfig.nullPlatform = config.nullPlatform;
        window_ = std::make_unique<Window>(events_, windowConfig);
    }

    RendererConfig rendererConfig;
    rendererConfig.backend = backend;
    rendererConfig.nullBackend = backend == RendererBackend::Null;
    rendererConfig.offscreen = config.offscreen;
    renderer_ = std::make_unique<Renderer>(window_.get(), rendererConfig);
    logger_.Write(Severity::Info, "RENDER",
                  std::string("Backend: ") + BackendName(renderer_->Backend()));

    assets_ = std::make_unique<AssetManager>(*jobs_);
    audio_ = std::make_unique<AudioSystem>(AudioConfig{
        config.audioEnabled && !config.headless && !config.nullPlatform && !config.offscreen,
        false, 48000});
    if (!audio_->Available())
        logger_.Write(Severity::Info, "AUDIO", audio_->Error());

    SetFPSLocked(true);
    resize_ = events_.Subscribe(EventType::FramebufferResize, [this](const Event &event) {
        renderer_->Resize(static_cast<unsigned>(event.x), static_cast<unsigned>(event.y));
    });
}

void Engine::SetFPSLocked(bool enabled)
{
    Thread::RequireMain();
    renderer_->SetVSync(false);
    limiter_.SetLocked(enabled);
}

bool Engine::IsFPSLocked() const
{
    Thread::RequireMain();
    return limiter_.IsLocked();
}

void Engine::PaceFrame(FrameLimiter::Clock::time_point start) const
{
    Thread::RequireMain();
    const auto delay = limiter_.Remaining(start, FrameLimiter::Clock::now());
    if (delay > FrameLimiter::Clock::duration::zero())
        std::this_thread::sleep_for(delay);
}

Engine::~Engine()
{
    assets_.reset();
    audio_.reset();
    jobs_.reset();
    resize_.Reset();
    renderer_.reset();
    window_.reset();
    logger_.Write(Severity::Info, "CORE", "Shutdown complete");
}
} // namespace PlagC
