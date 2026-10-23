#include <PlagC/World/SceneRenderer.h>
#include <PlagC/World/TransformSystem.h>
#include <PlagC/Thread.h>
#include <PlagC/Jobs.h>
#include "LegacySceneRenderer.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <numeric>
void TestWorld(PlagC::Renderer&,PlagC::JobSystem&);
void TestTransformSystem();
void TestSceneSynchronization(PlagC::Renderer&);
using namespace PlagC;
using Clock=std::chrono::steady_clock;
double Ms(Clock::time_point a,Clock::time_point b){return std::chrono::duration<double,std::milli>(b-a).count();}
double Median(std::vector<double> v){std::sort(v.begin(),v.end());return v[v.size()/2];}
void Benchmark(Renderer& r,unsigned n,bool fast) {
    auto mesh=r.CreateMesh(MakePrimitive(Primitive::Cube));auto material=r.CreateMaterial({});
    std::array<std::vector<double>,4> timings;
    {
        World world;std::vector<Entity> entities;entities.reserve(n);
        for(unsigned i=0;i<n;++i){auto e=world.Create("Benchmark cube "+std::to_string(i));world.SetRenderable(e,{mesh,material});entities.push_back(e);}
        SceneRenderer current(r);LegacySceneRenderer legacy(r);TransformSystem system;system.Bind(world,entities);
        if(fast)current.Sync(world);else legacy.Sync(world);
        for(unsigned frame=0;frame<55;++frame){
            const auto a=Clock::now();
            std::vector<TransformUpdate> snapshot;std::span<Transform> values;
            if(fast)values=system.Prepare();else snapshot=world.TransformSnapshot();
            const auto b=Clock::now();
            // Same deterministic transform workload; no jobs/driver noise in plumbing measurements.
            if(fast)for(auto& t:values)t.rotationDegrees.y+=1;
            else for(auto& u:snapshot)u.transform.rotationDegrees.y+=1;
            const auto c=Clock::now();
            if(fast)system.Commit();else world.ApplyTransforms(snapshot);
            const auto d=Clock::now();
            if(fast)current.Sync(world);else legacy.Sync(world);
            const auto e=Clock::now();
            if(frame>=5){timings[0].push_back(Ms(a,b));timings[1].push_back(Ms(c,d));timings[2].push_back(Ms(d,e));timings[3].push_back(Ms(a,e));}
        }
        if(fast){auto s=current.Statistics();if(s.transforms!=n||s.propertyCalls||s.structuralRebuild)throw std::runtime_error("unexpected fast-path renderer work");}
    }
    r.DestroyMaterial(material);r.DestroyMesh(mesh);
    std::cout<<n<<','<<(fast?"retained":"baseline");for(auto& t:timings)std::cout<<','<<std::fixed<<std::setprecision(4)<<Median(t);std::cout<<'\n';
}
int main(){try{
    Thread::InitializeMain();Renderer r(nullptr,{true});
    { JobSystem jobs(4);TestWorld(r,jobs); }
    TestTransformSystem();TestSceneSynchronization(r);
    std::cout<<"CPU recording backend; NOT Ogre or GPU/FPS. Median of 50 frames after 5 warmups.\n";
#ifdef NDEBUG
    std::cout<<"Build: optimized\n";
#else
    std::cout<<"Build: Debug/unoptimized\n";
#endif
    std::cout<<"cubes,path,gather_ms,commit_ms,sync_ms,total_ms\n";
    for(unsigned n:{1000u,3000u,7000u,10000u}){Benchmark(r,n,false);Benchmark(r,n,true);}
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
