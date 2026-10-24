#pragma once
#include <PlagC/Events.h>
#include <PlagC/Jobs.h>
#include <PlagC/Log.h>
#include <PlagC/Window.h>
#include <PlagC/Renderer/Renderer.h>
#include <memory>
#include <PlagC/FrameLimiter.h>
namespace PlagC {
struct EngineConfig { bool headless = false, nullPlatform = false, offscreen = false; unsigned workers = 0; unsigned frameLimit = 0; bool physicsLab = false; unsigned physicsWorkers = 2, physicsBodies = 1000;
    unsigned physicsScene=1,physicsQueries=1000,physicsTriangles=100000,physicsSeed=42,physicsWarmup=120,physicsSamples=600;
    bool physicsVariant=false,physicsIndividual=false,physicsBenchmark=false;
    double physicsHz=60;
    std::string physicsScheduler="native",physicsCSV="Logs/PhysicsLab.csv"; };
class Engine {
public:
    explicit Engine(const EngineConfig&);
    ~Engine();
    Log& Logger() { return logger_; }
    EventBus& Events() { return events_; }
    JobSystem& Jobs() { return *jobs_; }
    Renderer& Graphics() { return *renderer_; }
    Window* MainWindow() { return window_.get(); }
    // false removes engine pacing and requests VSync off; true caps at 60 FPS.
    void SetFPSLocked(bool enabled);
    bool IsFPSLocked() const;
    void PaceFrame(FrameLimiter::Clock::time_point start) const;
    void Stop() { running_ = false; }
    bool Running() const { return running_; }
private:
    Log logger_;
    EventBus events_;
    std::unique_ptr<JobSystem> jobs_;
    std::unique_ptr<Window> window_;
    std::unique_ptr<Renderer> renderer_;
    EventBus::Connection resize_;
    bool running_ = true;
    FrameLimiter limiter_;
};
class Application {
public:
    virtual ~Application() = default;
    virtual void OnStart(Engine&) = 0;
    virtual void OnFrame(Engine&, double deltaSeconds) = 0;
    virtual void OnStop(Engine&) {}
};
// Client implements this; engine provides main and owns initialization.
std::unique_ptr<Application> CreateApplication(const EngineConfig&);
}

