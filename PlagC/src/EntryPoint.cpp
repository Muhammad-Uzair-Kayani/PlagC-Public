#include "pgcpch.h"
#include <PlagC/Application.h>
#include <PlagC/Thread.h>
int main(int argc, char** argv) {
    try {
        PlagC::Thread::InitializeMain();
        PlagC::EngineConfig config;
        bool physicsWorkersExplicit=false, demoExplicit=false;
        for (int i=1;i<argc;++i) {
            const std::string arg = argv[i];
            if (arg == "--headless") config.headless = true;
            else if (arg == "--offscreen") config.offscreen = true;
            else if (arg == "--null") config.nullPlatform = true;
            else if (arg == "--physics") {config.physicsLab = true;demoExplicit=true;}
            else if (arg == "--integration") {config.physicsLab = false;demoExplicit=true;}
            else if (arg == "--physics-workers" && i+1<argc) {config.physicsWorkers = static_cast<unsigned>(std::stoul(argv[++i]));physicsWorkersExplicit=true;}
            else if (arg == "--physics-bodies" && i+1<argc) config.physicsBodies = static_cast<unsigned>(std::stoul(argv[++i]));
            else if (arg == "--physics-scene" && i+1<argc) {config.physicsScene=unsigned(std::stoul(argv[++i]));config.physicsLab=true;demoExplicit=true;}
            else if (arg == "--physics-queries" && i+1<argc) config.physicsQueries=unsigned(std::stoul(argv[++i]));
            else if (arg == "--physics-triangles" && i+1<argc) config.physicsTriangles=unsigned(std::stoul(argv[++i]));
            else if (arg == "--physics-seed" && i+1<argc) config.physicsSeed=unsigned(std::stoul(argv[++i]));
            else if (arg == "--physics-warmup" && i+1<argc) config.physicsWarmup=unsigned(std::stoul(argv[++i]));
            else if (arg == "--physics-samples" && i+1<argc) config.physicsSamples=unsigned(std::stoul(argv[++i]));
            else if (arg == "--physics-csv" && i+1<argc) config.physicsCSV=argv[++i];
            else if (arg == "--physics-scheduler" && i+1<argc) config.physicsScheduler=argv[++i];
            else if (arg == "--physics-hz" && i+1<argc) config.physicsHz=std::stod(argv[++i]);
            else if (arg == "--physics-variant") config.physicsVariant=true;
            else if (arg == "--physics-individual") config.physicsIndividual=true;
            else if (arg == "--physics-benchmark") {config.physicsLab=true;config.physicsBenchmark=true;demoExplicit=true;}
            else if (arg == "--frames" && i+1<argc) config.frameLimit = static_cast<unsigned>(std::stoul(argv[++i]));
            else if (arg == "--workers" && i+1<argc) config.workers = static_cast<unsigned>(std::stoul(argv[++i]));
            else throw std::invalid_argument("Usage: Sandbox [--headless|--null|--offscreen] [--frames N] [--workers N] [--physics|--integration] [--physics-workers N] [--physics-bodies N] [--physics-scene 1..14] [--physics-benchmark]. Full controls and flags: Docs/PhysicsLab.md");
        }
        // Preserve noninteractive regression runs; a normal F5/EXE launch opens the lab.
        if(!demoExplicit && (config.headless||config.nullPlatform||config.offscreen)) config.physicsLab=false;
        if(config.physicsBenchmark) config.physicsLab=true;
        if(unsigned(config.headless)+unsigned(config.nullPlatform)+unsigned(config.offscreen)>1) throw std::invalid_argument("choose one rendering mode");
        if(config.physicsScene<1||config.physicsScene>14||config.physicsQueries>10000||config.physicsTriangles>500000||!config.physicsSamples||config.physicsSamples>1000000||config.physicsWarmup>1000000||!std::isfinite(config.physicsHz)||config.physicsHz<1||config.physicsHz>240)throw std::invalid_argument("invalid Physics Lab workload; see Docs/PhysicsLab.md");
        if(config.physicsScheduler!="native"&&config.physicsScheduler!="plagc")throw std::invalid_argument("physics scheduler must be native or plagc");
        if(config.physicsWorkers>64||config.physicsBodies>16000)throw std::invalid_argument("physics workers <=64, bodies <=16000");
        if(config.physicsLab&&!physicsWorkersExplicit){
            const unsigned hardware=std::max(1u,std::thread::hardware_concurrency());
            config.physicsWorkers=std::min(config.physicsWorkers,hardware>2?hardware-2:0u);
        }
        if(config.physicsLab&&!config.workers){
            const unsigned hardware=std::max(1u,std::thread::hardware_concurrency());
            config.workers=config.physicsScheduler=="plagc"?std::min(32u,std::max(1u,hardware-1)):std::min(32u,hardware>config.physicsWorkers+1?hardware-config.physicsWorkers-1:1u);
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

