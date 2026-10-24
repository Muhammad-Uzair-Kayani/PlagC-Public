#include "pgcpch.h"
#include "../../Sandbox/src/StressSimulation.h"
#include "../../Sandbox/src/StressHud.h"
#include <PlagC/Jobs.h>
#include <PlagC/Thread.h>
#include <PlagC/World/SceneRenderer.h>
#include <cstdlib>
#include <fstream>
using namespace PlagC;
void Check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);std::cout<<"[PASS] "<<what<<std::endl;}
PhysicsConfig Config(JobSystem* jobs=nullptr){PhysicsConfig c;c.workers=2;c.maxBodies=12000;c.maxBodyPairs=262144;c.maxContacts=131072;if(jobs){c.scheduler=PhysicsScheduler::PlagC;c.sharedJobs=jobs;}return c;}
int main(){Thread::InitializeMain();try{
    {
        Renderer renderer(nullptr,{});StressHud hud;
        for(int i=0;i<4;++i){hud.Update(renderer,"FPS STRESS WORLD\nPHYSICS 2.30 MS");hud.Place(renderer,{0,2,5},-90,0,16.f/9);Check(renderer.Statistics().objects==2&&renderer.Statistics().meshes==2,"HUD text and crosshair replace resources without accumulation");}
        hud.Clear(renderer);Check(renderer.Statistics().objects==0&&renderer.Statistics().meshes==0&&renderer.Statistics().materials==0,"HUD teardown releases all resources");
    }
    for(bool shared:{false,true}){
        JobSystem jobs(2);Stress::Simulation sim({1000,42,false,true},Config(shared?&jobs:nullptr));
        Check(sim.world.Size()==1000,"initial entity ceiling includes environment, camera and lights");
        Check(sim.physics.Statistics().physics.constraints==27,"bridges and chains are actual Jolt constraints");
        for(int i=0;i<120;++i)sim.Advance(1./60);
        auto position=sim.Player().position;Check(sim.Player().grounded&&position.y>.7f&&position.y<1.2f,"physical player settles onto facility floor");
        sim.SetInput({0,0,-4},false,{0,0,-1});for(int i=0;i<60;++i)sim.Advance(1./60);Check(sim.Player().position.z<position.z-3,"WASD-style velocity moves physical player");
        sim.SetInput({},true,{0,0,-1});sim.Advance(1./60);Check(sim.Player().velocity.y>0,"grounded player jumps through CharacterVirtual");
        sim.ResetPlayer();for(int i=0;i<60;++i)sim.Advance(1./60);
        Entity cargo{};for(auto& v:sim.visuals)if(v.dynamic&&sim.world.Read(v.entity).name=="District cargo"){cargo=v.entity;break;}
        Check(bool(cargo),"generic cargo available for interaction");auto eye=sim.Eye();auto t=sim.world.ReadTransform(cargo);t.position={eye.x,eye.y,eye.z-2};sim.physics.Teleport(cargo,t);sim.physics.SetVelocity(cargo,{});
        sim.SetInput({},false,{0,0,-1});sim.Grab();Check(sim.Held()==cargo,"camera ray resolves arbitrary dynamic entity for grabbing");
        sim.SetInput({},false,{1,0,-1});sim.Advance(1./60);Check(sim.physics.Read(cargo).velocity.x>0,"holding applies physical force rather than visual teleport");
        sim.ThrowOrPush();Check(!sim.Held()&&sim.physics.Read(cargo).velocity.x>5,"throw adds mass-scaled impulse and releases held object");
        auto camera=sim.camera;auto first=sim.visuals.front().entity;sim.SetTarget(2500);Check(sim.camera==camera&&sim.world.Alive(first)&&sim.world.Size()==2500,"growth preserves world, camera and existing facility");
        sim.SetTarget(5000);Check(sim.world.Size()==5000,"high workload fills exact entity budget");sim.SetTarget(10000);Check(sim.world.Size()==10000,"extreme workload fills exact entity budget");
        sim.SetTarget(1000);Check(sim.world.Size()==1000,"downscaling destroys excess cargo and preserves permanent infrastructure");
        sim.Fire();Check(sim.world.Size()==1000,"projectile launch recycles within entity ceiling");
        auto time=sim.metrics.simulationSeconds;auto recycled=sim.metrics.recycled;for(int i=0;i<30;++i)sim.Advance(0);Check(sim.metrics.simulationSeconds==time&&sim.metrics.recycled==recycled,"pause freezes physics and workload clocks");
        sim.SetProgressive(true);for(int i=0;i<1801;++i)sim.Advance(1./60);Check(sim.Target()==1500&&sim.world.Size()==1500,"30 simulated seconds advances progressive stage without reload");
        Check(sim.metrics.recycled>recycled&&sim.metrics.disturbances>0,"factory and region impulses sustain activity with sleeping enabled");
    }
    if(const char* seconds=std::getenv("STRESS_SOAK_SECONDS")){
        double duration=std::stod(seconds);Check(duration>=1&&duration<=1800,"soak duration in supported bounds");unsigned population=1000;if(const char* count=std::getenv("STRESS_SOAK_ENTITIES"))population=unsigned(std::stoul(count));
        Stress::Simulation sim({population,42,false,true},Config());
        std::ofstream csv("Bin-Int/PhysicsTests/stress-soak.csv");csv<<"seconds,entities,bodies,active,sleeping,target,feasible,recycled,events\n";
        unsigned steps=unsigned(duration*60);for(unsigned i=0;i<steps;++i){sim.SetInput({},false,{0,0,-1});if(i%120==0)sim.Fire();sim.Advance(1./60);
            if(i%60==0){auto s=sim.physics.Statistics().physics;Check(sim.world.Size()==population&&s.bodies<=population,"soak entity/body bounds");auto p=sim.Player().position;if(!std::isfinite(p.y))throw std::runtime_error("non-finite player");csv<<i/60<<','<<sim.world.Size()<<','<<s.bodies<<','<<s.active<<','<<s.sleeping<<','<<sim.metrics.requestedActive<<','<<sim.metrics.feasibleActive<<','<<sim.metrics.recycled<<','<<sim.metrics.triggerEvents<<'\n';}
        }
    }
    std::cout<<"Stress world tests passed\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
