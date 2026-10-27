#include "pgcpch.h"
#include <PlagC/Application.h>
#include <PlagC/Thread.h>
#include <PlagC/PerformanceCapture.h>
int main(int argc, char **argv)
{
    try
    {
        PlagC::Thread::InitializeMain();
        PlagC::EngineConfig config; bool physicsWorkersExplicit = false;
        for (int i=1;i<argc;++i)
        {
            std::string arg=argv[i];
            auto value=[&]()->std::string{if(i+1>=argc)throw std::invalid_argument("missing value for "+arg);return argv[++i];};
            auto number=[&](){auto text=value();std::size_t used=0;auto n=std::stoul(text,&used);if(used!=text.size()||text.starts_with('-')||n>1000000)throw std::invalid_argument("invalid number for "+arg);return unsigned(n);};
            if(arg=="--performance-csv")config.performanceCSV=value();
            else if(arg=="--performance-warmup")config.performanceWarmup=number();
            else if(arg=="--performance-seconds"){const auto text=value();std::size_t used=0;config.performanceSeconds=std::stod(text,&used);if(used!=text.size())throw std::invalid_argument("invalid performance duration");}
            else if(arg=="--headless")config.headless=true;
            else if(arg=="--no-audio")config.audioEnabled=false;
            else if(arg=="--null")config.nullPlatform=true;
            else if(arg=="--offscreen")config.offscreen=true;
            else if(arg=="--frames")config.frameLimit=number();
            else if(arg=="--workers")config.workers=number();
            else if(arg=="--physics-workers"){config.physicsWorkers=number();physicsWorkersExplicit=true;}
            else if(arg=="--physics-hz")config.physicsHz=std::stod(value());
            else if(arg=="--physics-scheduler")config.physicsScheduler=value();
            else if(arg=="--help"){std::cout<<"OUTPOST ZERO: WASD move, mouse look, Shift sprint, Space jump, RMB aim, LMB fire, R reload, E interact, Escape mouse, Backspace redeploy.\nOptions: --headless | --null | --offscreen, --frames N, --workers N, --physics-workers N, --physics-hz N, --physics-scheduler native|plagc, --no-audio, --performance-csv PATH, --performance-warmup N, --performance-seconds N (capture then exit).\n";return 0;}
            else throw std::invalid_argument("Unknown option "+arg+". See Docs/TPS.md.");
        }
        if(unsigned(config.headless)+unsigned(config.nullPlatform)+unsigned(config.offscreen)>1)throw std::invalid_argument("choose one rendering mode");
        if(config.physicsWorkers>64||config.workers>32||!std::isfinite(config.physicsHz)||config.physicsHz<1||config.physicsHz>240)throw std::invalid_argument("invalid worker count or physics rate");
        if((config.headless||config.nullPlatform||config.offscreen)&&!config.frameLimit)config.frameLimit=120;
        if(config.physicsScheduler!="native"&&config.physicsScheduler!="plagc")throw std::invalid_argument("physics scheduler must be native or plagc");
        const unsigned hardware=std::max(1u,std::thread::hardware_concurrency());
        if(!physicsWorkersExplicit)config.physicsWorkers=std::min(config.physicsWorkers,hardware>2?hardware-2:0u);
        if(!config.workers)config.workers=config.physicsScheduler=="plagc"?std::min(32u,std::max(1u,hardware-1)):1u;
        if(!std::isfinite(config.performanceSeconds)||config.performanceSeconds<=0||config.performanceSeconds>3600)throw std::invalid_argument("performance duration must be in (0, 3600] seconds");
        const auto bootStart=std::chrono::steady_clock::now(); PlagC::Engine engine(config); auto application=PlagC::CreateApplication(config); if(!application)throw std::runtime_error("CreateApplication returned null");
        struct Drain{PlagC::JobSystem&jobs;~Drain(){jobs.Shutdown();}}drain{engine.Jobs()};
        PlagC::PerformanceCapture capture(config.performanceCSV,config.performanceWarmup,config.performanceSeconds);
        const auto loadStart=std::chrono::steady_clock::now();application->OnStart(engine);const auto ready=std::chrono::steady_clock::now();
        capture.Startup(std::chrono::duration<double,std::milli>(loadStart-bootStart).count(),std::chrono::duration<double,std::milli>(ready-loadStart).count(),config.workers,config.physicsWorkers,config.physicsScheduler,config.headless||config.nullPlatform||config.offscreen);
        auto previous=std::chrono::steady_clock::now();auto lastMeasurementEnd=previous;unsigned frames=0;
        auto ms=[](auto d){return std::chrono::duration<double,std::milli>(d).count();};
        while(engine.Running())
        {
            const auto frameStart=std::chrono::steady_clock::now();auto*window=engine.MainWindow();if(window)window->Poll();engine.Events().Pump();const auto pollEnd=std::chrono::steady_clock::now();if(window&&window->ShouldClose())break;
            const auto now=std::chrono::steady_clock::now();engine.Assets().Pump();const auto assetEnd=std::chrono::steady_clock::now();engine.timings={};PlagC::DetailedTimings()={};
            const auto updateStart=std::chrono::steady_clock::now();application->OnFrame(engine,std::chrono::duration<double>(now-previous).count());const auto updateEnd=std::chrono::steady_clock::now();
            engine.Graphics().Render(std::chrono::duration<double>(now-previous).count());const auto renderEnd=std::chrono::steady_clock::now();previous=now;engine.PaceFrame(frameStart);const auto frameEnd=std::chrono::steady_clock::now();
            const auto renderStats=engine.Graphics().Statistics();const double updateMs=ms(updateEnd-updateStart);const double knownGameMs=engine.timings.physicsMs+engine.timings.animationMs+engine.timings.syncMs+engine.timings.landscapeMs+engine.timings.audioMs+engine.timings.hudMs;
            auto detail=PlagC::DetailedTimings();
            PlagC::FrameProfile profile;profile.frameMs=ms(frameEnd-frameStart);profile.pollEventsMs=ms(pollEnd-frameStart);profile.assetPumpMs=ms(assetEnd-pollEnd);profile.gameUpdateMs=updateMs;profile.physicsMs=engine.timings.physicsMs;profile.animationMs=engine.timings.animationMs;profile.sceneSyncMs=engine.timings.syncMs;profile.landscapeMs=engine.timings.landscapeMs;profile.audioMs=engine.timings.audioMs;profile.hudMs=engine.timings.hudMs;profile.gameOtherMs=std::max(0.0,updateMs-knownGameMs);profile.renderSubmitMs=ms(renderEnd-updateEnd);profile.rendererCpuMs=renderStats.cpuMilliseconds;profile.pacingMs=ms(frameEnd-renderEnd);profile.unaccountedMs=std::max(0.0,profile.frameMs-profile.pollEventsMs-profile.assetPumpMs-profile.gameUpdateMs-profile.renderSubmitMs-profile.pacingMs);profile.animation=detail.animation;profile.animation.otherMs=std::max(0.0,profile.animationMs-profile.animation.poseMs-profile.animation.skinningMs-profile.animation.uploadMs);profile.renderer=detail.renderer;profile.renderer.renderFrameMs=renderStats.cpuMilliseconds;profile.renderer.otherMs=std::max(0.0,profile.renderSubmitMs-profile.renderer.renderFrameMs);profile.render=renderStats;
            engine.Profile().Submit(profile);
            if(engine.Profile().ConsumePublished())
            {
                const auto&p=engine.Profile().Latest();std::ostringstream out;out<<std::fixed<<std::setprecision(2)
                    <<"\nFRAME "<<p.frameMs<<" ms | "<<p.fps<<" FPS | P95 "<<p.frameP95Ms<<" | P99 "<<p.frameP99Ms<<" | Max "<<p.frameMaxMs<<" ms | Hitches >16.7 "<<p.hitch16Count<<" >33.3 "<<p.hitch33Count<<"\n"
                    <<"CPU\n"
                    <<"|- Events                 "<<p.pollEventsMs<<" ms | P99 "<<p.eventsP99Ms<<" | Max "<<p.eventsMaxMs<<"\n"
                    <<"|- Assets                 "<<p.assetPumpMs<<" ms\n"
                    <<"|- Game                   "<<p.gameUpdateMs<<" ms | P99 "<<p.gameP99Ms<<" | Max "<<p.gameMaxMs<<"\n"
                    <<"|  |- Physics             "<<p.physicsMs<<" ms\n"
                    <<"|  |- Animation           "<<p.animationMs<<" ms\n"
                    <<"|  |  |- Pose/Ozz         "<<p.animation.poseMs<<" ms\n"
                    <<"|  |  |- CPU Skinning     "<<p.animation.skinningMs<<" ms\n"
                    <<"|  |  |- Renderer Upload  "<<p.animation.uploadMs<<" ms\n"
                    <<"|  |  `- Other            "<<p.animation.otherMs<<" ms\n"
                    <<"|  |- Scene Sync          "<<p.sceneSyncMs<<" ms\n"
                    <<"|  |- Landscape           "<<p.landscapeMs<<" ms\n"
                    <<"|  |- Audio               "<<p.audioMs<<" ms\n"
                    <<"|  |- HUD                 "<<p.hudMs<<" ms\n"
                    <<"|  `- Other               "<<p.gameOtherMs<<" ms\n"
                    <<"`- Renderer               "<<p.renderSubmitMs<<" ms | P99 "<<p.renderP99Ms<<" | Max "<<p.renderMaxMs<<"\n"
                    <<"   |- Ogre renderOneFrame "<<p.renderer.renderFrameMs<<" ms\n"
                    <<"   `- Wrapper/Other       "<<p.renderer.otherMs<<" ms\n"
                    <<"NOTE: Ogre renderOneFrame is CPU wall time and may include driver/GPU wait; it is not a GPU timestamp.\n"
                    <<"RENDER STATS | Draws "<<p.render.drawCalls<<" | Tri "<<p.render.triangles<<" | Inst "<<p.render.instances<<" | Obj "<<p.render.objects<<" | Lights "<<p.render.lights;
                engine.Logger().Write(PlagC::Severity::Info,"PROFILER",out.str());
            }
            const bool done=capture.Frame(ms(frameEnd-lastMeasurementEnd),updateMs,renderStats,engine.timings.physicsMs,engine.timings.animationMs,engine.timings.syncMs,engine.IsFPSLocked(),engine.Graphics().Settings().vsync,window?window->Width():0,window?window->Height():0,window&&window->Minimized());lastMeasurementEnd=frameEnd;if(done||(config.frameLimit&&++frames>=config.frameLimit))break;
        }
        capture.Finish();if(!config.performanceCSV.empty())engine.Logger().Write(PlagC::Severity::Info,"PERF","Performance capture saved: "+config.performanceCSV);engine.Jobs().Shutdown();application->OnStop(engine);return 0;
    }
    catch(const std::exception&e){std::cerr<<"PlagC fatal: "<<e.what()<<'\n';return 1;}
}
