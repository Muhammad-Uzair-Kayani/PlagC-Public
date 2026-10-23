#include "pgcpch.h"
#include <PlagC/Application.h>
#include <PlagC/Thread.h>
int main(int argc, char** argv) {
    try {
        PlagC::Thread::InitializeMain();
        PlagC::EngineConfig config;
        for (int i=1;i<argc;++i) {
            const std::string arg = argv[i];
            if (arg == "--headless") config.headless = true;
            else if (arg == "--offscreen") config.offscreen = true;
            else if (arg == "--null") config.nullPlatform = true;
            else if (arg == "--frames" && i+1<argc) config.frameLimit = static_cast<unsigned>(std::stoul(argv[++i]));
            else if (arg == "--workers" && i+1<argc) config.workers = static_cast<unsigned>(std::stoul(argv[++i]));
            else throw std::invalid_argument("Usage: Sandbox [--headless|--null|--offscreen] [--frames N] [--workers N]");
        }
        if(unsigned(config.headless)+unsigned(config.nullPlatform)+unsigned(config.offscreen)>1) throw std::invalid_argument("choose one rendering mode");
        PlagC::Engine engine(config);
        auto application = PlagC::CreateApplication(config);
        if (!application) throw std::runtime_error("CreateApplication returned null");
        // Drain before destroying client state, including during exceptions.
        struct Drain { PlagC::JobSystem& jobs; ~Drain(){ jobs.Shutdown(); } } drain{engine.Jobs()};
        application->OnStart(engine);
        auto previous = std::chrono::steady_clock::now();
        unsigned frames = 0;
        while (engine.Running()) {
            const auto frameStart = std::chrono::steady_clock::now();
            auto* window = engine.MainWindow();
            if (window) window->Poll();
            engine.Events().Pump();
            if (window && window->ShouldClose()) break;
            const auto now = std::chrono::steady_clock::now();
            application->OnFrame(engine,std::chrono::duration<double>(now-previous).count());
            engine.Graphics().Render(std::chrono::duration<double>(now-previous).count());
            previous = now;
            if (config.frameLimit && ++frames >= config.frameLimit) break;
            engine.PaceFrame(frameStart);
        }
        engine.Jobs().Shutdown();
        application->OnStop(engine);
        return 0;
    } catch (const std::exception& e) { std::cerr << "PlagC fatal: " << e.what() << '\n'; return 1; }
}

