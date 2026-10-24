#include "pgcpch.h"
#include <PlagC/Application.h>
#include "StressWorld.h"
void TestPhysicsFoundation();
namespace {
class Sandbox final:public PlagC::Application {
    PlagC::EngineConfig config_;
    StressWorld stress_;
    unsigned frames_=0;
public:
    explicit Sandbox(const PlagC::EngineConfig& config):config_(config){}
    void OnStart(PlagC::Engine& engine)override{
        if(config_.selfTest)TestPhysicsFoundation();
        stress_.Start(engine,config_);
    }
    void OnFrame(PlagC::Engine& engine,double dt)override{
        stress_.Frame(engine,dt);
        if((config_.headless||config_.nullPlatform||config_.offscreen)&&!config_.frameLimit&&!config_.stressBenchmark&&!config_.stressSoak&&++frames_>=6)engine.Stop();
    }
    void OnStop(PlagC::Engine& engine)override{stress_.Stop(engine);}
};
}
std::unique_ptr<PlagC::Application> PlagC::CreateApplication(const PlagC::EngineConfig& config){return std::make_unique<Sandbox>(config);}
