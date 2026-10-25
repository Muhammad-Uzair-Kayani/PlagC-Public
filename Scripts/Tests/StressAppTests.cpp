// Real Engine/application/StressWorld code with recording graphics and no native OS window.
#include "pgcpch.h"
#include <PlagC/Application.h>
#include <PlagC/Thread.h>
#include <PlagC/Input/Input.h>
#include "../../Sandbox/src/StressWorld.h"
#include <fstream>
namespace PlagC
{
InputState replayInput;
void Window::Poll()
{
    Input::state_ = replayInput;
}
struct Log::Impl
{
};
Log::Log() : impl_(std::make_unique<Impl>())
{
}
Log::~Log() = default;
void Log::Write(Severity, std::string_view, std::string_view)
{
}
struct Window::Impl
{
};
Window::Window(EventBus &, const WindowConfig &) : impl_(std::make_unique<Impl>())
{
}
Window::~Window() = default;
void Window::SetCursorCaptured(bool)
{
}
void Window::SetTitle(const std::string &)
{
}
int Window::Width() const
{
    return 1280;
}
int Window::Height() const
{
    return 720;
}
} // namespace PlagC
using namespace PlagC;
void Check(bool ok, const char *what)
{
    if (!ok)
        throw std::runtime_error(what);
    std::cout << "[PASS] " << what << std::endl;
}
int main()
{
    Thread::InitializeMain();
    try
    {
        EngineConfig c;
        c.assetDemo = false;
        c.headless = true;
        c.workers = 2;
        c.stressProgressive = false;
        Engine engine(c);
        StressWorld app;
        app.Start(engine, c);
        app.Frame(engine, 1. / 60);
        auto base = engine.Graphics().Statistics();
        Check(base.objects > 900 && base.materials == 14,
              "FPS world binds procedural geometry and HUD with recording backend");
        auto press = [&](KeyCode key, int mods = 0) {
            Event event{EventType::KeyPressed};
            event.key = key;
            event.modifiers = mods;
            engine.Events().Post(event);
            engine.Events().Pump();
            app.Frame(engine, 0);
        };
        press(KeyCode::D2);
        Check(engine.Graphics().Statistics().objects == base.objects + 1500,
              "queued preset key grows the same rendered world");
        press(KeyCode::B);
        Check(engine.Graphics().Statistics().objects == base.objects + 1600,
              "queued B adds 100 entities");
        press(KeyCode::V);
        Check(engine.Graphics().Statistics().objects == base.objects + 2100,
              "queued V adds 500 entities");
        press(KeyCode::F2);
        press(KeyCode::F4);
        Check(engine.Graphics().Settings().wireframe, "debug and wireframe keys reach renderer");
        press(KeyCode::P);
        press(KeyCode::N);
        press(KeyCode::F1);
        press(KeyCode::F3);
        press(KeyCode::E);
        press(KeyCode::G);
        press(KeyCode::F);
        press(KeyCode::R);
        for (int i = 0; i < 3; ++i)
        {
            press(KeyCode::R, 1);
            auto stats = engine.Graphics().Statistics();
            Check(stats.objects == base.objects && stats.meshes == base.meshes &&
                      stats.materials == base.materials,
                  "world reset releases prior render resources before rebuilding");
        }
        app.Stop(engine);
        auto empty = engine.Graphics().Statistics();
        Check(!empty.objects && !empty.meshes && !empty.materials && !empty.lights,
              "StressWorld stop releases all graphics handles");
        c.stressBenchmark = true;
        c.stressSeconds = 1;
        c.stressWarmup = 2;
        c.stressCSV = "Bin-Int/PhysicsTests/stress-app.csv";
        Engine bench(c);
        StressWorld benchmark;
        benchmark.Start(bench, c);
        unsigned frames = 0;
        while (bench.Running() && frames++ < 100)
        {
            benchmark.Frame(bench, 1. / 60);
            bench.Graphics().Render(1. / 60);
        }
        benchmark.Stop(bench);
        Check(frames == 62, "benchmark exits after exact warmup and sample counts");
        std::ifstream csv(c.stressCSV);
        std::string line;
        std::getline(csv, line);
        auto columns = std::count(line.begin(), line.end(), ',');
        unsigned rows = 0;
        while (std::getline(csv, line))
        {
            if (std::count(line.begin(), line.end(), ',') != columns)
                throw std::runtime_error("CSV column mismatch");
            ++rows;
        }
        Check(rows == 60, "benchmark CSV has complete sample rows");
        std::ifstream summary(c.stressCSV + ".summary.csv");
        rows = 0;
        while (std::getline(summary, line))
            ++rows;
        Check(rows == 15, "benchmark summary contains all fourteen timing distributions");
        c.stressBenchmark = false;
        c.stressSoak = true;
        c.stressWarmup = 0;
        c.stressCSV = "Bin-Int/PhysicsTests/stress-soak-app.csv";
        Engine soak(c);
        StressWorld soakApp;
        soakApp.Start(soak, c);
        frames = 0;
        while (soak.Running() && frames++ < 100)
            soakApp.Frame(soak, .02);
        Check(frames == 50,
              "interactive soak stops on elapsed time instead of fixed-tick sample count");
        Check(soak.Graphics().Statistics().objects == base.objects,
              "interactive soak retains HUD and crosshair");
        soakApp.Stop(soak);
        c.stressSoak = false;
        c.assetDemo = true;
        c.headless = false;
        c.nullPlatform = true;
        Engine demo(c);
        StressWorld demoApp;
        demoApp.Start(demo, c);
        demo.Assets().Wait();
        demoApp.Frame(demo, 1. / 60);
        auto demoStats = demo.Graphics().Statistics();
        auto expectedMaterials =
            14 + demo.Assets().Get<ModelAsset>("Beacon.plasset")->materials.size() +
            demo.Assets().Get<ModelAsset>("RobotExpressive.plasset")->materials.size() +
            demo.Assets().Get<ModelAsset>("MaterialLab.plasset")->materials.size();
        Check(
            demoStats.textures == 7 && demoStats.materials == expectedMaterials,
            "default facility finalizes cached imported texture/model alongside procedural world");
        auto key = [&](KeyCode k, int modifiers = 0) {
            Event event{EventType::KeyPressed};
            event.key = k;
            event.modifiers = modifiers;
            demo.Events().Post(event);
            demo.Events().Pump();
        };
        auto advance = [&](unsigned frames) {
            for (unsigned i = 0; i < frames; ++i)
            {
                demo.MainWindow()->Poll();
                demoApp.Frame(demo, 1. / 60);
            }
        };
        advance(120);
        Check(demoApp.LocomotionState() == "Idle", "stationary character enters Idle");
        Event movement{EventType::KeyPressed};
        movement.key = KeyCode::W;
        replayInput.Apply(movement);
        advance(45);
        Check(demoApp.LocomotionState() == "Walk", "actual character velocity drives Walk");
        movement.key = KeyCode::LeftShift;
        replayInput.Apply(movement);
        advance(45);
        Check(demoApp.LocomotionState() == "Run", "sprint velocity drives Run");
        replayInput.Reset();
        advance(90);
        Check(demoApp.LocomotionState() == "Idle", "stopping returns character to Idle");
        key(KeyCode::F8);
        Check(demoApp.AssetStatus().find("FOLLOW CAMERA") != std::string::npos,
              "F8 selects follow camera");
        key(KeyCode::F9);
        Check(demoApp.AssetStatus().find("FREE CAMERA") != std::string::npos,
              "F9 selects free camera");
        key(KeyCode::F9);
        key(KeyCode::F5);
        Check(demoApp.AssetStatus() == "F5 BEACON ANIMATION PAUSED", "F5 confirms animation pause");
        key(KeyCode::F5);
        Check(demoApp.AssetStatus() == "F5 BEACON ANIMATION PLAYING",
              "F5 confirms animation resume");
        key(KeyCode::F6);
        Check(demoApp.AssetStatus() == "F6 AUDIO UNAVAILABLE - SEE CONSOLE",
              "F6 explains unavailable headless audio");
        key(KeyCode::F7);
        Check(demoApp.AssetStatus() == "F7 RELOADING COOKED ASSETS", "F7 confirms reload request");
        demo.Assets().Wait();
        demoApp.Frame(demo, 1. / 60);
        Check(demo.Graphics().Statistics().textures == 7,
              "hot reload replaces GPU texture without accumulation");
        key(KeyCode::R, 1);
        demoApp.Frame(demo, 1. / 60);
        Check(demo.Graphics().Statistics().textures == 7 &&
                  demo.Graphics().Statistics().materials == expectedMaterials,
              "world reset recreates asset instances without resource accumulation");
        demoApp.Stop(demo);
        auto cleaned = demo.Graphics().Statistics();
        Check(!cleaned.objects && !cleaned.meshes && !cleaned.materials && !cleaned.textures,
              "integrated asset showcase tears down all backend resources");
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
