#pragma once

#include <PlagC/Assets/AssetManager.h>
#include <PlagC/Audio/AudioSystem.h>
#include <PlagC/Events.h>
#include <PlagC/FrameLimiter.h>
#include <PlagC/Jobs.h>
#include <PlagC/Log.h>
#include <PlagC/Profiler.h>
#include <PlagC/Renderer/Renderer.h>
#include <PlagC/Window.h>
#include <memory>
#include <string>

namespace PlagC
{
struct EngineConfig
{
    RendererBackend rendererBackend = RendererBackend::Auto;
    // The console selector is a legacy/debug surface. Normal clients start without stdin.
    bool rendererSelectionPrompt = false;

    std::string performanceCSV;
    unsigned performanceWarmup = 120;
    double performanceSeconds = 60.0;

    bool headless = false;
    bool nullPlatform = false;
    bool offscreen = false;
    bool audioEnabled = true;

    unsigned workers = 0;
    unsigned frameLimit = 0;

    // Shared defaults for clients that create PhysicsSystem instances.
    unsigned physicsWorkers = 2;
    double physicsHz = 120.0;
    std::string physicsScheduler = "native";

    // Legacy validation-harness settings. They are consumed only by test programs under
    // Scripts/Tests and are intentionally not interpreted by Engine.
    bool assetDemo = true;
    unsigned stressEntities = 1000;
    unsigned stressSeed = 42;
    unsigned stressWarmup = 120;
    double stressSeconds = 600.0;
    bool stressProgressive = true;
    bool stressBenchmark = false;
    bool stressSoak = false;
    bool selfTest = false;
    std::string stressCSV = "Logs/StressWorld.csv";
};

class Engine
{
  public:
    explicit Engine(const EngineConfig &);
    ~Engine();

    // Optional application-reported engine subsystem timings. EntryPoint measures the
    // complete application update independently, so clients never need to fill these.
    struct SystemTimings
    {
        double physicsMs = 0.0;
        double animationMs = 0.0;
        double sceneSyncMs = 0.0;
        double audioMs = 0.0;
    } timings;

    Log &Logger() { return logger_; }
    EventBus &Events() { return events_; }
    JobSystem &Jobs() { return *jobs_; }
    Renderer &Graphics() { return *renderer_; }
    AssetManager &Assets() { return *assets_; }
    AudioSystem &Audio() { return *audio_; }
    Profiler &Profile() { return profiler_; }
    const Profiler &Profile() const { return profiler_; }
    Window *MainWindow() { return window_.get(); }

    void SetFPSLocked(bool enabled);
    bool IsFPSLocked() const;
    void PaceFrame(FrameLimiter::Clock::time_point start) const;

    void Stop() { running_ = false; }
    bool Running() const { return running_; }

  private:
    Log logger_;
    EventBus events_;
    Profiler profiler_;
    std::unique_ptr<JobSystem> jobs_;
    std::unique_ptr<Window> window_;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<AssetManager> assets_;
    std::unique_ptr<AudioSystem> audio_;
    EventBus::Connection resize_;
    bool running_ = true;
    FrameLimiter limiter_;
};

class Application
{
  public:
    virtual ~Application() = default;
    virtual void OnStart(Engine &) = 0;
    virtual void OnFrame(Engine &, double) = 0;
    virtual void OnStop(Engine &) {}
};

// Implemented by a client executable such as Sandbox, Editor, a game, or a tool.
// The engine owns main() and invokes this factory after Engine initialization.
std::unique_ptr<Application> CreateApplication(const EngineConfig &);
} // namespace PlagC
