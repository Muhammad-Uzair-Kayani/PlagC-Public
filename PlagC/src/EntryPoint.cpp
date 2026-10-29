#include "pgcpch.h"
#include <PlagC/Application.h>
#include <PlagC/GpuProfiler.h>
#include <PlagC/LatencyProfiler.h>
#include <PlagC/PerformanceCapture.h>
#include <PlagC/Thread.h>
#include <iomanip>

namespace
{
using Clock = std::chrono::steady_clock;

double Milliseconds(Clock::duration duration)
{
    return std::chrono::duration<double, std::milli>(duration).count();
}

unsigned ParseUnsigned(const std::string &argument, int &index, int argc, char **argv)
{
    if (index + 1 >= argc)
        throw std::invalid_argument("missing value for " + argument);
    const std::string text = argv[++index];
    std::size_t used = 0;
    const auto value = std::stoul(text, &used);
    if (used != text.size() || text.starts_with('-') || value > 1000000)
        throw std::invalid_argument("invalid number for " + argument);
    return static_cast<unsigned>(value);
}

std::string ParseString(const std::string &argument, int &index, int argc, char **argv)
{
    if (index + 1 >= argc)
        throw std::invalid_argument("missing value for " + argument);
    return argv[++index];
}
} // namespace

int main(int argc, char **argv)
{
    try
    {
        PlagC::Thread::InitializeMain();

        PlagC::EngineConfig config;
        bool physicsWorkersExplicit = false;

        for (int i = 1; i < argc; ++i)
        {
            const std::string argument = argv[i];
            if (argument == "--performance-csv")
                config.performanceCSV = ParseString(argument, i, argc, argv);
            else if (argument == "--performance-warmup")
                config.performanceWarmup = ParseUnsigned(argument, i, argc, argv);
            else if (argument == "--performance-seconds")
            {
                const auto text = ParseString(argument, i, argc, argv);
                std::size_t used = 0;
                config.performanceSeconds = std::stod(text, &used);
                if (used != text.size())
                    throw std::invalid_argument("invalid performance duration");
            }
            else if (argument == "--headless")
                config.headless = true;
            else if (argument == "--no-audio")
                config.audioEnabled = false;
            else if (argument == "--null")
                config.nullPlatform = true;
            else if (argument == "--offscreen")
                config.offscreen = true;
            else if (argument == "--frames")
                config.frameLimit = ParseUnsigned(argument, i, argc, argv);
            else if (argument == "--workers")
                config.workers = ParseUnsigned(argument, i, argc, argv);
            else if (argument == "--physics-workers")
            {
                config.physicsWorkers = ParseUnsigned(argument, i, argc, argv);
                physicsWorkersExplicit = true;
            }
            else if (argument == "--physics-hz")
                config.physicsHz =
                    std::stod(ParseString(argument, i, argc, argv));
            else if (argument == "--physics-scheduler")
                config.physicsScheduler = ParseString(argument, i, argc, argv);
            else if (argument == "--renderer")
                config.rendererBackend =
                    PlagC::ParseRendererBackend(ParseString(argument, i, argc, argv));
            else if (argument == "--help")
            {
                std::cout
                    << "PlagC runtime options:\n"
                    << "  --renderer auto|opengl|vulkan|null\n"
                    << "  --headless | --null | --offscreen\n"
                    << "  --frames N\n"
                    << "  --workers N\n"
                    << "  --physics-workers N\n"
                    << "  --physics-hz N\n"
                    << "  --physics-scheduler native|plagc\n"
                    << "  --no-audio\n"
                    << "  --performance-csv PATH\n"
                    << "  --performance-warmup N\n"
                    << "  --performance-seconds N\n";
                return 0;
            }
            else
            {
                throw std::invalid_argument("unknown option: " + argument);
            }
        }

        if (unsigned(config.headless) + unsigned(config.nullPlatform) +
                unsigned(config.offscreen) >
            1)
            throw std::invalid_argument("choose only one rendering mode");

        if (config.physicsWorkers > 64 || config.workers > 32 ||
            !std::isfinite(config.physicsHz) || config.physicsHz < 1.0 ||
            config.physicsHz > 240.0)
            throw std::invalid_argument("invalid worker count or physics rate");

        if ((config.headless || config.nullPlatform || config.offscreen) &&
            !config.frameLimit)
            config.frameLimit = 120;

        if (config.physicsScheduler != "native" &&
            config.physicsScheduler != "plagc")
            throw std::invalid_argument(
                "physics scheduler must be native or plagc");

        if (!std::isfinite(config.performanceSeconds) ||
            config.performanceSeconds <= 0.0 || config.performanceSeconds > 3600.0)
            throw std::invalid_argument(
                "performance duration must be in (0, 3600] seconds");

        const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());
        if (!physicsWorkersExplicit)
            config.physicsWorkers =
                std::min(config.physicsWorkers, hardware > 2 ? hardware - 2 : 0u);
        if (!config.workers)
            config.workers = config.physicsScheduler == "plagc"
                                 ? std::min(32u, std::max(1u, hardware - 1))
                                 : 1u;

        const auto bootStart = Clock::now();
        PlagC::Engine engine(config);
        auto application = PlagC::CreateApplication(config);
        if (!application)
            throw std::runtime_error("CreateApplication returned null");

        struct JobDrain
        {
            PlagC::JobSystem &jobs;
            ~JobDrain() { jobs.Shutdown(); }
        } jobDrain{engine.Jobs()};

        PlagC::GpuProfiler gpuProfiler(engine.Graphics().Backend());
        PlagC::PerformanceCapture capture(
            config.performanceCSV, config.performanceWarmup, config.performanceSeconds);

        const auto applicationStart = Clock::now();
        application->OnStart(engine);
        bool applicationStarted = true;

        struct ApplicationStop
        {
            std::unique_ptr<PlagC::Application> &application;
            PlagC::Engine &engine;
            bool &started;
            ~ApplicationStop()
            {
                if (started && application)
                {
                    try
                    {
                        application->OnStop(engine);
                    }
                    catch (...)
                    {
                    }
                }
            }
        } stopGuard{application, engine, applicationStarted};

        const auto ready = Clock::now();
        capture.Startup(
            Milliseconds(applicationStart - bootStart),
            Milliseconds(ready - applicationStart), config.workers,
            config.physicsWorkers, config.physicsScheduler,
            config.headless || config.nullPlatform || config.offscreen);

        auto previous = ready;
        auto lastMeasurementEnd = ready;
        unsigned frames = 0;

        while (engine.Running())
        {
            PlagC::LatencyProfiler::BeginFrame();
            const auto frameStart = Clock::now();

            auto *window = engine.MainWindow();
            if (window)
                window->Poll();
            engine.Events().Pump();
            const auto pollEnd = Clock::now();

            if (window && window->ShouldClose())
                break;

            const auto now = Clock::now();
            engine.Assets().Pump();
            const auto assetEnd = Clock::now();

            engine.timings = {};
            PlagC::DetailedTimings() = {};

            const auto updateStart = Clock::now();
            const double deltaSeconds =
                std::chrono::duration<double>(now - previous).count();
            application->OnFrame(engine, deltaSeconds);
            const auto updateEnd = Clock::now();
            PlagC::LatencyProfiler::OnSceneSync();

            gpuProfiler.BeginFrame();
            engine.Graphics().Render(deltaSeconds);
            gpuProfiler.EndFrame();
            PlagC::LatencyProfiler::OnRenderSubmit();
            const auto renderEnd = Clock::now();

            previous = now;
            engine.PaceFrame(frameStart);
            const auto frameEnd = Clock::now();

            const auto renderStats = engine.Graphics().Statistics();
            const auto gpu = gpuProfiler.Latest();
            PlagC::LatencyProfiler::OnGpuTiming(
                gpu.supported, gpu.valid, gpu.totalMs, gpu.latencyFrames,
                gpu.outstandingFrames);

            const double updateMs = Milliseconds(updateEnd - updateStart);
            const double knownApplicationMs =
                engine.timings.physicsMs + engine.timings.animationMs +
                engine.timings.sceneSyncMs + engine.timings.audioMs;

            const auto detail = PlagC::DetailedTimings();
            PlagC::FrameProfile profile;
            profile.frameMs = Milliseconds(frameEnd - frameStart);
            profile.pollEventsMs = Milliseconds(pollEnd - frameStart);
            profile.assetPumpMs = Milliseconds(assetEnd - pollEnd);
            profile.applicationUpdateMs = updateMs;
            profile.physicsMs = engine.timings.physicsMs;
            profile.animationMs = engine.timings.animationMs;
            profile.sceneSyncMs = engine.timings.sceneSyncMs;
            profile.audioMs = engine.timings.audioMs;
            profile.applicationOtherMs =
                std::max(0.0, updateMs - knownApplicationMs);
            profile.renderSubmitMs = Milliseconds(renderEnd - updateEnd);
            profile.rendererCpuMs = renderStats.cpuMilliseconds;
            profile.pacingMs = Milliseconds(frameEnd - renderEnd);
            profile.unaccountedMs =
                std::max(0.0,
                         profile.frameMs - profile.pollEventsMs -
                             profile.assetPumpMs - profile.applicationUpdateMs -
                             profile.renderSubmitMs - profile.pacingMs);

            profile.animation = detail.animation;
            profile.animation.otherMs =
                std::max(0.0,
                         profile.animationMs - profile.animation.poseMs -
                             profile.animation.skinningMs -
                             profile.animation.uploadMs);
            profile.renderer = detail.renderer;
            profile.renderer.renderFrameMs = renderStats.cpuMilliseconds;
            profile.renderer.otherMs =
                std::max(0.0,
                         profile.renderSubmitMs -
                             profile.renderer.renderFrameMs);
            profile.gpuSupported = gpu.supported;
            profile.gpuValid = gpu.valid;
            profile.gpuTotalMs = gpu.totalMs;
            profile.gpuLatencyFrames = gpu.latencyFrames;
            profile.render = renderStats;
            engine.Profile().Submit(profile);

            if (engine.Profile().ConsumePublished())
            {
                const auto latency = PlagC::LatencyProfiler::ConsumeOneSecond();
                const auto gpuPasses = gpuProfiler.ConsumePassStats();
                const auto &published = engine.Profile().Latest();

                std::ostringstream output;
                output << std::fixed << std::setprecision(3)
                       << "\nFRAME " << published.frameMs << " ms | "
                       << published.fps << " FPS | P95 "
                       << published.frameP95Ms << " | P99 "
                       << published.frameP99Ms << " | Max "
                       << published.frameMaxMs << " | Hitches >16.7 "
                       << published.hitch16Count << " >33.3 "
                       << published.hitch33Count << '\n';

                output << "LATENCY | frame " << latency.frame
                       << " | transitions " << latency.transitions
                       << " | releases " << latency.releases
                       << " | mouse " << latency.mouseMoves << '\n';

                const char *stageNames[] = {
                    "Input read",      "Simulation input", "Physics input",
                    "Physics step",    "Physics output",   "World transform",
                    "Camera",          "Animation",        "Scene sync",
                    "Render submit"};
                for (unsigned i = 0;
                     i < static_cast<unsigned>(PlagC::LatencyStage::Count); ++i)
                {
                    const auto &stage = latency.stages[i];
                    output << "|- " << std::left << std::setw(20) << stageNames[i]
                           << std::right << stage.averageMs << " ms | P99 "
                           << stage.p99Ms << " | Max " << stage.maxMs
                           << " | Frames avg " << stage.averageFrames
                           << " max " << stage.maxFrames << " | N "
                           << stage.samples << '\n';
                }

                output << "|- Physics fixed step     steps "
                       << latency.physicsSteps << " | alpha "
                       << latency.physicsAlpha << " | dropped "
                       << latency.physicsDroppedMs << " ms\n"
                       << "|- Oldest tracked input   "
                       << latency.oldestPendingMs << " ms\n";

                if (latency.gpuSupported && latency.gpuValid)
                    output << "`- GPU async              " << latency.gpuMs
                           << " ms | query age "
                           << latency.gpuQueryLatencyFrames << " | queued "
                           << latency.gpuOutstandingFrames << '\n';
                else
                    output << "`- GPU async              "
                           << (latency.gpuSupported ? "waiting" : "unsupported")
                           << '\n';

                output << "CPU\n"
                       << "|- Platform/events        "
                       << published.pollEventsMs << " ms | P99 "
                       << published.eventsP99Ms << " | Max "
                       << published.eventsMaxMs << '\n'
                       << "|- Asset pump             "
                       << published.assetPumpMs << " ms\n"
                       << "|- Application update     "
                       << published.applicationUpdateMs << " ms | P99 "
                       << published.applicationP99Ms << " | Max "
                       << published.applicationMaxMs << '\n'
                       << "|  |- Physics             "
                       << published.physicsMs << " ms\n"
                       << "|  |- Animation           "
                       << published.animationMs << " ms\n"
                       << "|  |  |- Pose/Ozz         "
                       << published.animation.poseMs << " ms\n"
                       << "|  |  |- CPU skinning     "
                       << published.animation.skinningMs << " ms\n"
                       << "|  |  |- Renderer upload  "
                       << published.animation.uploadMs << " ms\n"
                       << "|  |  `- Other            "
                       << published.animation.otherMs << " ms\n"
                       << "|  |- Scene sync          "
                       << published.sceneSyncMs << " ms\n"
                       << "|  |- Audio               "
                       << published.audioMs << " ms\n"
                       << "|  `- Other application   "
                       << published.applicationOtherMs << " ms\n"
                       << "|- Renderer submit        "
                       << published.renderSubmitMs << " ms | P99 "
                       << published.renderP99Ms << " | Max "
                       << published.renderMaxMs << '\n'
                       << "|  |- Ogre renderOneFrame "
                       << published.renderer.renderFrameMs << " ms\n"
                       << "|  `- Wrapper/driver      "
                       << published.renderer.otherMs << " ms\n"
                       << "|- Frame pacing           "
                       << published.pacingMs << " ms\n"
                       << "`- Frame unaccounted      "
                       << published.unaccountedMs << " ms\n";

                if (published.gpuSupported && published.gpuValid)
                {
                    output << "GPU\n|- Total frame             "
                           << published.gpuTotalMs << " ms | P99 "
                           << published.gpuP99Ms << " | Max "
                           << published.gpuMaxMs << " | N "
                           << published.gpuSamples << '\n';
                    const char *passNames[] = {
                        "Shadows",          "Main scene/PBS",
                        "Sky/environment",   "Bloom extract",
                        "Bloom downsample",  "Bloom blur H",
                        "Bloom blur V",      "Bloom upsample",
                        "Tone map / FXAA",   "Overlay"};
                    for (std::size_t i = 0; i < PlagC::GpuPassCount; ++i)
                    {
                        const auto &pass = gpuPasses[i];
                        output << (i + 1 == PlagC::GpuPassCount ? "`- " : "|- ")
                               << std::left << std::setw(20) << passNames[i]
                               << std::right << pass.averageMs << " ms | P95 "
                               << pass.p95Ms << " | P99 " << pass.p99Ms
                               << " | Max " << pass.maxMs << " | N "
                               << pass.samples << '\n';
                    }
                }
                else
                {
                    output << "GPU\n`- "
                           << (published.gpuSupported
                                   ? "waiting for asynchronous timestamps"
                                   : "unsupported by active backend")
                           << '\n';
                }

                output << "RENDER STATS | Draws "
                       << published.render.drawCalls << " | Tri "
                       << published.render.triangles << " | Inst "
                       << published.render.instances << " | Obj "
                       << published.render.objects << " | Lights "
                       << published.render.lights;

                engine.Logger().Write(
                    PlagC::Severity::Info, "PROFILER", output.str());
            }

            const bool captureDone = capture.Frame(
                Milliseconds(frameEnd - lastMeasurementEnd), updateMs,
                renderStats, engine.timings.physicsMs,
                engine.timings.animationMs, engine.timings.sceneSyncMs,
                engine.IsFPSLocked(), engine.Graphics().Settings().vsync,
                window ? window->Width() : 0,
                window ? window->Height() : 0,
                window && window->Minimized());
            lastMeasurementEnd = frameEnd;

            ++frames;
            if (captureDone ||
                (config.frameLimit && frames >= config.frameLimit))
                break;
        }

        capture.Finish();
        applicationStarted = false;
        application->OnStop(engine);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::cerr << "PlagC fatal error: " << error.what() << '\n';
        return 1;
    }
}
