#include "pgcpch.h"
#include <PlagC/Application.h>
#include <PlagC/Thread.h>
int main(int argc, char** argv) {
    try {
        PlagC::Thread::InitializeMain();
        PlagC::EngineConfig config;
        bool physicsWorkersExplicit=false;
        for(int i=1;i<argc;++i){
            std::string arg=argv[i];
            auto value=[&]()->std::string {if(i+1>=argc)throw std::invalid_argument("missing value for "+arg);return argv[++i];};
            auto number=[&]()->unsigned {auto text=value();std::size_t used=0;auto n=std::stoul(text,&used);if(used!=text.size()||text.starts_with('-')||n>1000000)throw std::invalid_argument("invalid number for "+arg);return unsigned(n);};
            if(arg=="--headless")config.headless=true;
            else if(arg=="--null")config.nullPlatform=true;
            else if(arg=="--offscreen")config.offscreen=true;
            else if(arg=="--frames")config.frameLimit=number();
            else if(arg=="--workers")config.workers=number();
            else if(arg=="--physics-workers"){config.physicsWorkers=number();physicsWorkersExplicit=true;}
            else if(arg=="--physics-hz")config.physicsHz=std::stod(value());
            else if(arg=="--physics-scheduler")config.physicsScheduler=value();
            else if(arg=="--entities")config.stressEntities=number();
            else if(arg=="--seed")config.stressSeed=number();
            else if(arg=="--no-progressive")config.stressProgressive=false;
            else if(arg=="--soak")config.stressSoak=true;
            else if(arg=="--benchmark")config.stressBenchmark=true;
            else if(arg=="--seconds")config.stressSeconds=std::stod(value());
            else if(arg=="--warmup")config.stressWarmup=number();
            else if(arg=="--csv")config.stressCSV=value();
            else if(arg=="--self-test")config.selfTest=true;
            else if(arg=="--stress"||arg=="--physics"){} // Compatibility alias; only one playable world exists.
            else throw std::invalid_argument("Unknown option "+arg+". See Docs/StressWorld.md; the old scene selector has been removed.");
        }
        if(config.stressSoak&&config.stressBenchmark)throw std::invalid_argument("choose --soak or --benchmark");
        if(unsigned(config.headless)+unsigned(config.nullPlatform)+unsigned(config.offscreen)>1)throw std::invalid_argument("choose one rendering mode");
        if(config.stressEntities<1000||config.stressEntities>10000||config.physicsWorkers>64||config.workers>32||!std::isfinite(config.physicsHz)||config.physicsHz<1||config.physicsHz>240||!std::isfinite(config.stressSeconds)||config.stressSeconds<1||config.stressSeconds>1800||config.stressWarmup>10000)throw std::invalid_argument("invalid stress workload limits; see Docs/StressWorld.md");
        if(config.physicsScheduler!="native"&&config.physicsScheduler!="plagc")throw std::invalid_argument("physics scheduler must be native or plagc");
        const unsigned hardware=std::max(1u,std::thread::hardware_concurrency());
        if(!physicsWorkersExplicit)config.physicsWorkers=std::min(config.physicsWorkers,hardware>2?hardware-2:0u);
        if(!config.workers)config.workers=config.physicsScheduler=="plagc"?std::min(32u,std::max(1u,hardware-1)):1u;
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

