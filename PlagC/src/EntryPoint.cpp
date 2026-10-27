#include "pgcpch.h"
#include <PlagC/Application.h>
#include <PlagC/Thread.h>
#include <PlagC/PerformanceCapture.h>
int main(int argc, char **argv)
{
    try
    {
        PlagC::Thread::InitializeMain();
        PlagC::EngineConfig config;
        bool physicsWorkersExplicit = false;
        for (int i = 1; i < argc; ++i)
        {
            std::string arg = argv[i];
            auto value = [&]() -> std::string {
                if (i + 1 >= argc)
                    throw std::invalid_argument("missing value for " + arg);
                return argv[++i];
            };
            auto number = [&]() -> unsigned {
                auto text = value();
                std::size_t used = 0;
                auto n = std::stoul(text, &used);
                if (used != text.size() || text.starts_with('-') || n > 1000000)
                    throw std::invalid_argument("invalid number for " + arg);
                return unsigned(n);
            };
            if (arg == "--performance-csv")
                config.performanceCSV = value();
            else if (arg == "--performance-warmup")
                config.performanceWarmup = number();
            else if (arg == "--performance-seconds")
            {
                const auto text = value();
                std::size_t used = 0;
                config.performanceSeconds = std::stod(text, &used);
                if (used != text.size())
                    throw std::invalid_argument("invalid performance duration");
            }
            else if (arg == "--headless")
                config.headless = true;
            else if (arg == "--no-audio")
                config.audioEnabled = false;
            else if (arg == "--null")
                config.nullPlatform = true;
            else if (arg == "--offscreen")
                config.offscreen = true;
            else if (arg == "--frames")
                config.frameLimit = number();
            else if (arg == "--workers")
                config.workers = number();
            else if (arg == "--physics-workers")
            {
                config.physicsWorkers = number();
                physicsWorkersExplicit = true;
            }
            else if (arg == "--physics-hz")
                config.physicsHz = std::stod(value());
            else if (arg == "--physics-scheduler")
                config.physicsScheduler = value();
            else if (arg == "--help")
            {
                std::cout
                    << "OUTPOST ZERO: WASD move, mouse look, Shift sprint, Space jump, RMB aim, "
                       "LMB fire, R reload, E interact, Escape mouse, Backspace redeploy.\n"
                       "Options: --headless | --null | --offscreen, --frames N, --workers N, "
                       "--physics-workers N, --physics-hz N, --physics-scheduler native|plagc, "
                       "--no-audio, --performance-csv PATH, --performance-warmup N, "
                       "--performance-seconds N (capture then exit).\n";
                return 0;
            }
            else
                throw std::invalid_argument("Unknown option " + arg + ". See Docs/TPS.md.");
        }
        if (unsigned(config.headless) + unsigned(config.nullPlatform) + unsigned(config.offscreen) >
            1)
            throw std::invalid_argument("choose one rendering mode");
        if (config.physicsWorkers > 64 || config.workers > 32 || !std::isfinite(config.physicsHz) ||
            config.physicsHz < 1 || config.physicsHz > 240)
            throw std::invalid_argument("invalid worker count or physics rate");
        if ((config.headless || config.nullPlatform || config.offscreen) && !config.frameLimit)
            config.frameLimit = 120;
        if (config.physicsScheduler != "native" && config.physicsScheduler != "plagc")
            throw std::invalid_argument("physics scheduler must be native or plagc");
        const unsigned hardware = std::max(1u, std::thread::hardware_concurrency());
        if (!physicsWorkersExplicit)
            config.physicsWorkers =
                std::min(config.physicsWorkers, hardware > 2 ? hardware - 2 : 0u);
        if (!config.workers)
            config.workers =
                config.physicsScheduler == "plagc" ? std::min(32u, std::max(1u, hardware - 1)) : 1u;
        if (!std::isfinite(config.performanceSeconds) || config.performanceSeconds <= 0 ||
            config.performanceSeconds > 3600)
            throw std::invalid_argument("performance duration must be in (0, 3600] seconds");
        const auto bootStart = std::chrono::steady_clock::now();
        PlagC::Engine engine(config);
        auto application = PlagC::CreateApplication(config);
        if (!application)
            throw std::runtime_error("CreateApplication returned null");
        // Drain before destroying client state, including during exceptions.
        struct Drain
        {
            PlagC::JobSystem &jobs;
            ~Drain()
            {
                jobs.Shutdown();
            }
        } drain{engine.Jobs()};
        PlagC::PerformanceCapture capture(config.performanceCSV, config.performanceWarmup,
                                         config.performanceSeconds);
        const auto loadStart = std::chrono::steady_clock::now();
        application->OnStart(engine);
        const auto ready = std::chrono::steady_clock::now();
        capture.Startup(std::chrono::duration<double, std::milli>(loadStart - bootStart).count(),
                        std::chrono::duration<double, std::milli>(ready - loadStart).count(),
                        config.workers, config.physicsWorkers, config.physicsScheduler,
                        config.headless || config.nullPlatform || config.offscreen);
        auto previous = std::chrono::steady_clock::now();
        auto lastMeasurementEnd = previous;
        unsigned frames = 0;
        while (engine.Running())
        {
            const auto frameStart = std::chrono::steady_clock::now();
            auto *window = engine.MainWindow();
            if (window)
                window->Poll();
            engine.Events().Pump();
            if (window && window->ShouldClose())
                break;
            const auto now = std::chrono::steady_clock::now();
            engine.Assets().Pump();
            engine.timings = {};
            const auto updateStart = std::chrono::steady_clock::now();
            application->OnFrame(engine, std::chrono::duration<double>(now - previous).count());
            const auto updateEnd = std::chrono::steady_clock::now();
            engine.Graphics().Render(std::chrono::duration<double>(now - previous).count());
            previous = now;
            engine.PaceFrame(frameStart);
            const auto frameEnd = std::chrono::steady_clock::now();
            auto ms = [](auto duration) {
                return std::chrono::duration<double, std::milli>(duration).count();
            };
            const bool done = capture.Frame(ms(frameEnd - lastMeasurementEnd),
                ms(updateEnd - updateStart), engine.Graphics().Statistics(),
                engine.timings.physicsMs, engine.timings.animationMs, engine.timings.syncMs,
                engine.IsFPSLocked(), engine.Graphics().Settings().vsync,
                window ? window->Width() : 0, window ? window->Height() : 0,
                window && window->Minimized());
            lastMeasurementEnd = frameEnd;
            if (done || (config.frameLimit && ++frames >= config.frameLimit))
                break;
        }
        capture.Finish();
        if (!config.performanceCSV.empty())
            engine.Logger().Write(PlagC::Severity::Info, "PERF",
                                  "Performance capture saved: " + config.performanceCSV);
        engine.Jobs().Shutdown();
        application->OnStop(engine);
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "PlagC fatal: " << e.what() << '\n';
        return 1;
    }
}
