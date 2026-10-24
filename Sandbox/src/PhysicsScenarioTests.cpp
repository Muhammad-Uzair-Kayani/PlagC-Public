#include "pgcpch.h"
#include "PhysicsScenarios.h"
#include <PlagC/Jobs.h>
#ifdef PC_PHYSICS_RECORDING_TEST
#include "PhysicsHud.h"
#endif
using namespace PlagC;
void TestPhysicsScenarios(){
#ifdef PC_PHYSICS_RECORDING_TEST
 {Renderer renderer(nullptr,{});PhysicsHud hud;for(int i=0;i<5;++i){hud.Update(renderer,"SCENE 14 RAGDOLLS\nFPS 120.00");hud.Place(renderer,{0,3,5},-90,-20,16.f/9);auto s=renderer.Statistics();if(s.objects!=1||s.meshes!=1||s.materials!=1)throw std::runtime_error("HUD resource leak");}hud.Clear(renderer);if(renderer.Statistics().objects||renderer.Statistics().meshes||renderer.Statistics().materials)throw std::runtime_error("HUD teardown leak");std::cout<<"[PASS] HUD mesh replacement and teardown\n";}
#endif
 JobSystem jobs(2);
 for(unsigned variant=0;variant<2;++variant)for(unsigned number=0;number<PhysicsLabScenes::Count;++number){
  PhysicsLabScenes::Options o;o.scene=number;o.bodies=30;o.triangles=128;o.queries=30;o.alternate=variant!=0;o.individual=number==11&&variant!=0;
  PhysicsConfig config;config.workers=2;config.maxBodies=4096;if(variant){config.scheduler=PhysicsScheduler::PlagC;config.sharedJobs=&jobs;}
  PhysicsLabScenes::Scene scene(o,config);
  for(const auto& visual:scene.visuals){auto mesh=scene.physics.BuildColliderMesh(visual.collider);if(mesh.vertices.empty()||mesh.indices.empty())throw std::runtime_error("empty collider visualization");for(auto index:mesh.indices)if(index>=mesh.vertices.size())throw std::runtime_error("invalid collider mesh index");for(auto v:mesh.vertices)if(!std::isfinite(v.position.x)||!std::isfinite(v.normal.y))throw std::runtime_error("invalid collider vertex");}
  for(int step=0;step<90;++step){scene.SetFocus({float(step-45),0,-49});scene.Advance(1./60);}
  scene.Query(30);scene.Fire({0,10,0},{1,0,0});scene.Advance(1./60);
  if(number==11){auto before=scene.physics.Statistics().physics.bodies;scene.Stream();if(scene.physics.Statistics().physics.bodies!=before)throw std::runtime_error("streaming leaked bodies");}
  if(number==8&&scene.enters==0)throw std::runtime_error("static sensor did not detect kinematic agent");
  for(auto e:scene.dynamics)if(scene.world.Alive(e)){auto t=scene.world.ReadTransform(e);if(!std::isfinite(t.position.y))throw std::runtime_error("non-finite scene transform");}
  std::cout<<"[PASS] scene "<<number+1<<" "<<PhysicsLabScenes::Names[number]<<" variant "<<variant<<" mesh/step/query/lifecycle\n";
 }
}
#ifdef PC_PHYSICS_TEST_MAIN
#include <fstream>
#include <iomanip>
void BenchmarkPhysicsCPU(const char* path){
 std::ofstream csv(path);if(!csv)throw std::runtime_error("cannot open CPU benchmark CSV");
 csv<<"scene,scheduler,workers,frame,bodies,active,sleeping,preparation_ms,physics_ms,input_ms,output_ms,query_ms,events_ms,characters_ms,structure_ms\n";
 for(unsigned workers:{1u,2u,4u,6u,11u})for(unsigned scheduler=0;scheduler<2;++scheduler){JobSystem jobs(workers);
  for(unsigned number=0;number<PhysicsLabScenes::Count;++number){
   PhysicsLabScenes::Options options;options.scene=number;options.bodies=number==9?8:500;options.queries=500;options.triangles=10000;
   PhysicsConfig config;config.workers=workers;config.maxBodies=4096;config.maxContacts=32768;if(scheduler){config.scheduler=PhysicsScheduler::PlagC;config.sharedJobs=&jobs;}
   PhysicsLabScenes::Scene scene(options,config);
   for(unsigned frame=0;frame<180;++frame){auto start=std::chrono::steady_clock::now();scene.SetFocus({float(std::sin(frame*.01)*40),0,0});scene.Advance(1./60);if(number==7)scene.Query(options.queries);double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();auto s=scene.physics.Statistics();auto p=s.physics;
    if(frame>=60)csv<<number+1<<','<<(scheduler?"plagc":"native")<<','<<workers<<','<<frame-60<<','<<p.bodies<<','<<p.active<<','<<p.sleeping<<','<<ms<<','<<s.stepMs<<','<<s.inputMs<<','<<s.outputMs<<','<<scene.queryMs<<','<<s.eventsMs<<','<<s.charactersMs<<','<<s.structureMs<<'\n';
   }
   std::cout<<"[CPU BENCH] scene="<<number+1<<" workers="<<workers<<" scheduler="<<scheduler<<std::endl;
  }
 }
 if(!csv)throw std::runtime_error("CPU benchmark write failed");
}
#endif
