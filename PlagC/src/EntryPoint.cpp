#include "pgcpch.h"
#include <PlagC/Application.h>
#include <PlagC/Thread.h>
int main(int argc, char** argv) {
    try {
        PlagC::Thread::InitializeMain();
        PlagC::EngineConfig config;
        bool physicsWorkersExplicit=false;
        for (int i=1;i<argc;++i) {
            const std::string arg = argv[i];
            if (arg == "--headless") config.headless = true;
            else if (arg == "--offscreen") config.offscreen = true;
            else if (arg == "--null") config.nullPlatform = true;
            else if (arg == "--physics") config.physicsLab = true;
            else if (arg == "--physics-workers" && i+1<argc) {config.physicsWorkers = static_cast<unsigned>(std::stoul(argv[++i]));physicsWorkersExplicit=true;}
            else if (arg == "--physics-bodies" && i+1<argc) config.physicsBodies = static_cast<unsigned>(std::stoul(argv[++i]));
            else if (arg == "--frames" && i+1<argc) config.frameLimit = static_cast<unsigned>(std::stoul(argv[++i]));
            else if (arg == "--workers" && i+1<argc) config.workers = static_cast<unsigned>(std::stoul(argv[++i]));
            else throw std::invalid_argument("Usage: Sandbox [--headless|--null|--offscreen] [--frames N] [--workers N] [--physics] [--physics-workers N] [--physics-bodies N]");
        }
        if(unsigned(config.headless)+unsigned(config.nullPlatform)+unsigned(config.offscreen)>1) throw std::invalid_argument("choose one rendering mode");
        if(config.physicsWorkers>64||config.physicsBodies>16000)throw std::invalid_argument("physics workers <=64, bodies <=16000");
        if(config.physicsLab&&!physicsWorkersExplicit){
            const unsigned hardware=std::max(1u,std::thread::hardware_concurrency());
            config.physicsWorkers=std::min(config.physicsWorkers,hardware>2?hardware-2:0u);
        }
        if(config.physicsLab&&!config.workers){
            const unsigned hardware=std::max(1u,std::thread::hardware_concurrency());
            config.workers=std::min(32u,hardware>config.physicsWorkers+1?hardware-config.physicsWorkers-1:1u);
        }
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

