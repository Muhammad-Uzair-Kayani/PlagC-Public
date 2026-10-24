#include "pgcpch.h"
#include "PhysicsLab.h"
#include "PhysicsLabControls.h"
#include <PlagC/Input/Input.h>
#include <iomanip>
using namespace PlagC;
namespace {
using Clock=std::chrono::steady_clock;
double Ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
bool Same(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
bool Same(const ColliderComponent& a,const ColliderComponent& b){return a.type==b.type&&Same(a.dimensions,b.dimensions)&&a.geometry==b.geometry&&Same(a.centerOfMassOffset,b.centerOfMassOffset);}
double Quantile(std::vector<double> values,double q){if(values.empty())return 0;std::sort(values.begin(),values.end());return values[std::min(values.size()-1,std::size_t(q*double(values.size()-1)))];}
}
void PhysicsLab::Start(Engine& engine,const EngineConfig& config){
 config_=config;engine.SetFPSLocked(false);options_={config.physicsScene-1,config.physicsBodies,config.physicsQueries,config.physicsTriangles,config.physicsSeed,config.physicsVariant,config.physicsIndividual};
 physicsConfig_.workers=config.physicsWorkers;physicsConfig_.fixedHz=config.physicsHz;physicsConfig_.maxBodies=40000;physicsConfig_.maxBodyPairs=262144;physicsConfig_.maxContacts=131072;
 physicsConfig_.scheduler=config.physicsScheduler=="plagc"?PhysicsScheduler::PlagC:PhysicsScheduler::Native;physicsConfig_.sharedJobs=&engine.Jobs();benchmark_=config.physicsBenchmark;
 keys_=engine.Events().Subscribe(EventType::KeyPressed,[this,&engine](const Event& e){Key(engine,e);});Load(engine);
}
void PhysicsLab::Unload(Engine& engine){
 auto& r=engine.Graphics();hud_.Clear(r);for(auto h:marks_)r.DestroyObject(h);marks_.clear();scene_.reset();model_.reset();
 for(auto h:materials_){r.DestroyMaterial(h);}
 for(auto h:meshes_){r.DestroyMesh(h);}
 materials_.clear();meshes_.clear();cached_.clear();bound_.clear();markMesh_={};
}
void PhysicsLab::Load(Engine& engine){
 if(csv_.is_open()){FinishBenchmark();}
 if(benchmark_){position_={0,32,65};yaw_=-90;pitch_=-25;colours_=false;}Unload(engine);auto start=Clock::now();auto& r=engine.Graphics();model_=std::make_unique<PhysicsLabScenes::Scene>(options_,physicsConfig_);scene_=std::make_unique<SceneRenderer>(r);
 for(auto colour:{Vec3{.2f,.55f,.95f},Vec3{1,.4f,.15f},Vec3{.3f,.9f,.4f},Vec3{.4f,.45f,.5f},Vec3{.85f,.25f,.9f},Vec3{.15f,.3f,.6f},Vec3{1,.9f,.2f}}){MaterialDesc d;d.colour=colour;materials_.push_back(r.CreateMaterial(d));}
 markMesh_=r.CreateMesh(MakePrimitive(Primitive::Cube));meshes_.push_back(markMesh_);
 camera_=model_->world.Create("Lab camera");model_->world.SetTransform(camera_,{position_,{},{1,1,1}});model_->world.SetCamera(camera_,{{0,0,0},60,.1f,2000});model_->world.SetActiveCamera(camera_);auto sun=model_->world.Create("Lab sun");model_->world.SetLight(sun,{});
 Visuals(r);Camera(engine,0);scene_->Sync(model_->world,model_->physics.Presentation());loadMs_=Ms(start);frame_=0;hudTime_=1;visualTime_=0;frameTimes_.clear();physicsTimes_.clear();syncTimes_.clear();
 std::cout<<"[LAB] "<<options_.scene+1<<" "<<PhysicsLabScenes::Names[options_.scene]<<" loadMs="<<loadMs_<<" seed="<<options_.seed<<" bodies="<<model_->physics.Statistics().physics.bodies<<" Jolt workers="<<model_->physics.Statistics().physics.workers<<" PlagC workers="<<engine.Jobs().WorkerCount()<<" unlocked render / "<<physicsConfig_.fixedHz<<" Hz physics\n";
 if(benchmark_){std::filesystem::path path(config_.physicsCSV);if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());csv_.open(path);if(!csv_)throw std::runtime_error("cannot open Physics Lab CSV");csv_<<"scene,variant,scheduler,workers,seed,fixed_hz,frame,bodies,static,dynamic,kinematic,active,sleeping,contacts,constraints,sensors,characters,steps,dropped_s,frame_ms,preparation_ms,previous_render_ms,physics_ms,input_ms,output_ms,presentation_ms,sync_ms,query_ms,event_ms,character_ms,stream_ms,lod_ms,ecs_writes,enters,stays,exits,load_ms,raycasts_total,shape_casts_total,overlaps_total,structure_ms\n";}
}
void PhysicsLab::Visuals(Renderer& r){
 for(const auto& v:model_->visuals){
  if(!model_->world.Alive(v.entity))continue;
  auto binding=bound_.find(v.entity.value);
  unsigned colour=v.colour;if(colours_&&v.dynamic&&model_->world.Read(v.entity).physics)colour=model_->physics.Read(v.entity).active?0:5;
  MeshHandle mesh{};for(const auto& cached:cached_)if(Same(v.collider,cached.collider)){mesh=cached.mesh;break;}
  if(!mesh){mesh=r.CreateMesh(model_->physics.BuildColliderMesh(v.collider));meshes_.push_back(mesh);cached_.push_back({v.collider,mesh});}
  auto material=materials_.at(colour%materials_.size());if(binding==bound_.end()||binding->second.mesh.value!=mesh.value||binding->second.material.value!=material.value){RenderableComponent c{mesh,material,true,v.colour!=4};model_->world.SetRenderable(v.entity,c);bound_[v.entity.value]=c;}
 }
 std::erase_if(bound_,[&](auto& b){return !model_->world.Alive({b.first});});
}
void PhysicsLab::Key(Engine& e,const Event& event){
 const auto key=event.key;
 std::cout<<"[LAB INPUT] key="<<unsigned(key)<<" modifiers="<<event.modifiers<<(benchmark_?" benchmark: controls disabled":"")<<std::endl;
 if(key==KeyCode::Escape){e.Stop();return;}
 if(benchmark_)return; // Timed runs must not be mutated by interactive controls.
 bool reload=false;
 if(auto scene=PhysicsLabControls::SceneForPress(event,options_.scene)){
  options_.scene=*scene;reload=true;
  std::cout<<"[LAB INPUT] selecting "<<options_.scene+1<<" "<<PhysicsLabScenes::Names[options_.scene]<<std::endl;
 }
 if(key==KeyCode::R)reload=true;
 if(key==KeyCode::V){if(options_.scene==11)options_.individual=!options_.individual;else options_.alternate=!options_.alternate;reload=true;}
 if(key==KeyCode::Insert){options_.bodies=std::min(16000u,options_.bodies+500);reload=true;}
 if(key==KeyCode::Delete){options_.bodies=options_.bodies>500?options_.bodies-500:100;reload=true;}
 if(key==KeyCode::J){physicsConfig_.scheduler=physicsConfig_.scheduler==PhysicsScheduler::Native?PhysicsScheduler::PlagC:PhysicsScheduler::Native;reload=true;}
 if(key==KeyCode::O){physicsConfig_.fixedHz=physicsConfig_.fixedHz==60?30:physicsConfig_.fixedHz==30?90:60;reload=true;}
 if(key==KeyCode::Space)paused_=!paused_;
 if(key==KeyCode::N){paused_=true;singleStep_=true;}
 if(key==KeyCode::H){help_=!help_;hudTime_=1;}
 if(key==KeyCode::C){auto s=e.Graphics().Settings();s.wireframe=!s.wireframe;e.Graphics().SetSettings(s);}
 if(key==KeyCode::K){colours_=!colours_;visualTime_=1;}
 if(key==KeyCode::M&&options_.scene==11){model_->Stream();visualTime_=1;}
 if(key==KeyCode::F){model_->Fire(position_,forward_);visualTime_=1;}
 if(key==KeyCode::B){benchmark_=true;reload=true;}
 if(reload)Load(e);
}
void PhysicsLab::Camera(Engine& e,double dt){
 const bool look=!benchmark_&&Input::IsMouseButtonPressed(MouseCode::Right);if(look&&looking_){auto delta=Input::GetMouseDelta();yaw_+=float(delta.x)*.12f;pitch_=std::clamp(pitch_-float(delta.y)*.12f,-89.f,89.f);}if(look!=looking_)if(auto* window=e.MainWindow())window->SetCursorCaptured(look);looking_=look;
 constexpr float rad=3.14159265358979323846f/180;forward_={std::cos(yaw_*rad)*std::cos(pitch_*rad),std::sin(pitch_*rad),std::sin(yaw_*rad)*std::cos(pitch_*rad)};Vec3 right{-std::sin(yaw_*rad),0,std::cos(yaw_*rad)},move{};
 auto add=[&](Vec3 v,float sign){move.x+=v.x*sign;move.y+=v.y*sign;move.z+=v.z*sign;};if(Input::IsKeyPressed(KeyCode::W))add(forward_,1);if(Input::IsKeyPressed(KeyCode::S))add(forward_,-1);if(Input::IsKeyPressed(KeyCode::A))add(right,-1);if(Input::IsKeyPressed(KeyCode::D))add(right,1);if(Input::IsKeyPressed(KeyCode::Q))move.y-=1;if(Input::IsKeyPressed(KeyCode::E))move.y+=1;
 auto length=std::sqrt(move.x*move.x+move.y*move.y+move.z*move.z);if(length>0&&!benchmark_){float speed=float(std::min(dt,.1))*(Input::IsKeyPressed(KeyCode::LeftShift)?60:15)/length;add({},0);position_.x+=move.x*speed;position_.y+=move.y*speed;position_.z+=move.z*speed;}
 auto& w=model_->world;w.SetCamera(camera_,{{position_.x+forward_.x,position_.y+forward_.y,position_.z+forward_.z},60,.1f,2000});w.SetTransform(camera_,{position_,{},{1,1,1}});model_->SetFocus(benchmark_?Vec3{float(std::sin(double(frame_)/physicsConfig_.fixedHz*.3)*40),0,0}:position_);
 Vec3 input{};if(Input::IsKeyPressed(KeyCode::Up))input.z-=3;if(Input::IsKeyPressed(KeyCode::Down))input.z+=3;if(Input::IsKeyPressed(KeyCode::Left))input.x-=3;if(Input::IsKeyPressed(KeyCode::Right))input.x+=3;model_->SetCharacterMovement(benchmark_?Vec3{0,0,2}:input,!benchmark_&&Input::IsKeyPressed(KeyCode::Enter));
}
void PhysicsLab::Hud(Engine& e,double dt){
 const auto s=model_->physics.Statistics();const auto& p=s.physics;std::ostringstream text;text<<std::fixed<<std::setprecision(2);
 text<<options_.scene+1<<" "<<PhysicsLabScenes::Names[options_.scene]<<"  "<<(paused_?"PAUSED":"RUNNING")<<"\n";
 text<<"MODE "<<(options_.alternate?"ALTERNATE":"DEFAULT")<<" INSERT "<<(options_.individual?"INDIVIDUAL":"BATCH")<<"\n";
 text<<"FPS "<<(dt>0?1/dt:0)<<" FRAME "<<dt*1000<<" MS  PHYSICS "<<physicsConfig_.fixedHz<<" HZ\n";
 text<<"BODIES "<<p.bodies<<" STATIC "<<p.statics<<" DYNAMIC "<<p.dynamics<<" KIN "<<p.kinematics<<"\n";
 text<<"ACTIVE "<<p.active<<" SLEEP "<<p.sleeping<<" CONTACTS "<<p.contacts<<"\n";
 text<<"JOINTS "<<p.constraints<<" SENSORS "<<p.sensors<<" CHARACTERS "<<p.characters<<"\n";
 text<<"STEP "<<s.stepMs<<" IN "<<s.inputMs<<" OUT "<<s.outputMs<<" MS\n";
 text<<"SYNC "<<scene_->Statistics().totalMs<<" QUERY "<<model_->queryMs<<" EVENTS "<<s.eventsMs<<" MS\n";
 text<<"RAYS TOTAL "<<p.raycasts<<" CASTS "<<p.shapeCasts<<" OVERLAPS "<<p.overlaps<<"\n";
 text<<"ENTER "<<model_->enters<<" STAY "<<model_->stays<<" EXIT "<<model_->exits<<"\n";
 text<<"WORKERS "<<p.workers<<" "<<(physicsConfig_.scheduler==PhysicsScheduler::Native?"NATIVE":"PLAGC")<<" STEPS "<<s.steps<<" DROP "<<s.droppedSeconds<<" S\n";
 if(help_){text<<"F1 SHAPES  F2 MATERIALS  F3 CCD  F4 PYRAMID\nF5 ISLANDS  F6 SLEEPING CITY  F7 STATIC WORLD\nF8 QUERIES  F9 TRIGGERS  F10 CHARACTER\nF11 CONSTRAINTS  F12 STREAMING\nSHIFT F1 LOD  SHIFT F2 RAGDOLLS\nPGUP PGDN SCENES  V VARIANT  INS DEL COUNT\nSPACE PAUSE  N STEP  R RESET  F FIRE\nWASD QE CAMERA  RMB LOOK  SHIFT FAST\nARROWS CHARACTER  ENTER JUMP\nC WIREFRAME  K SLEEP COLOURS  H HELP\nJ SCHEDULER  O HZ  M STREAM  B BENCHMARK";}
 hud_.Update(e.Graphics(),text.str());if(auto* w=e.MainWindow())w->SetTitle("PlagC Physics Lab | "+std::string(PhysicsLabScenes::Names[options_.scene])+" | H: controls | FPS "+std::to_string(int(dt>0?1/dt:0)));
 std::cout<<"[LAB] "<<PhysicsLabScenes::Names[options_.scene]<<" active="<<p.active<<" sleeping="<<p.sleeping<<" physicsMs="<<s.stepMs<<" syncMs="<<scene_->Statistics().totalMs<<" queryMs="<<model_->queryMs<<'\n';
}
void PhysicsLab::Sample(Engine& e,double wallMs,double measuredMs){
 if(!benchmark_||frame_++<config_.physicsWarmup)return;
 const auto s=model_->physics.Statistics();const auto& p=s.physics;const auto sync=scene_->Statistics().totalMs;
 frameTimes_.push_back(measuredMs);physicsTimes_.push_back(s.stepMs);syncTimes_.push_back(sync);
 csv_<<options_.scene+1<<','<<options_.alternate<<','<<(physicsConfig_.scheduler==PhysicsScheduler::Native?"native":"plagc")<<','<<p.workers<<','<<options_.seed<<','<<physicsConfig_.fixedHz<<','<<frameTimes_.size()<<','<<p.bodies<<','<<p.statics<<','<<p.dynamics<<','<<p.kinematics<<','<<p.active<<','<<p.sleeping<<','<<p.contacts<<','<<p.constraints<<','<<p.sensors<<','<<p.characters<<','<<s.steps<<','<<s.droppedSeconds<<','<<wallMs<<','<<measuredMs<<','<<e.Graphics().Statistics().cpuMilliseconds<<','<<s.stepMs<<','<<s.inputMs<<','<<s.outputMs<<','<<s.presentationMs<<','<<sync<<','<<model_->queryMs<<','<<s.eventsMs<<','<<s.charactersMs<<','<<model_->streamMs<<','<<model_->lodMs<<','<<s.ecsWrites<<','<<model_->enters<<','<<model_->stays<<','<<model_->exits<<','<<loadMs_<<','<<p.raycasts<<','<<p.shapeCasts<<','<<p.overlaps<<','<<s.structureMs<<'\n';
 if(frameTimes_.size()>=config_.physicsSamples){FinishBenchmark();e.Stop();}
}
void PhysicsLab::FinishBenchmark(){if(!csv_.is_open())return;csv_.close();if(!csv_)throw std::runtime_error("Physics Lab CSV write failed");std::cout<<"[BENCHMARK] samples="<<frameTimes_.size()<<" CPU preparation median/p95="<<Quantile(frameTimes_,.5)<<'/'<<Quantile(frameTimes_,.95)<<" physics median/p95="<<Quantile(physicsTimes_,.5)<<'/'<<Quantile(physicsTimes_,.95)<<" sync median/p95="<<Quantile(syncTimes_,.5)<<'/'<<Quantile(syncTimes_,.95)<<" CSV="<<config_.physicsCSV<<'\n';}
void PhysicsLab::Frame(Engine& e,double dt){
 auto start=Clock::now();Camera(e,dt);const double simulationDt=benchmark_?1/physicsConfig_.fixedHz:singleStep_?1/physicsConfig_.fixedHz:paused_?0:dt;singleStep_=false;model_->Advance(simulationDt);
 if(options_.scene==7)model_->Query(options_.queries);else model_->queryMs=0;
 visualTime_+=dt;if(visualTime_>=.2||bound_.size()!=model_->visuals.size()||options_.scene==12){Visuals(e.Graphics());visualTime_=0;}
 scene_->Sync(model_->world,model_->physics.Presentation());
 // Draw a bounded sample of query results; benchmark runs omit diagnostic draw work.
 if(!benchmark_&&options_.scene==7){for(auto h:marks_)e.Graphics().DestroyObject(h);marks_.clear();for(auto q:model_->queryMarks){auto h=e.Graphics().CreateObject(markMesh_,materials_[q.hit?2:1],{{q.start.x,(q.start.y+q.end.y)/2,q.start.z},{},{.025f,std::max(.025f,q.start.y-q.end.y),.025f}});e.Graphics().SetCastShadows(h,false);marks_.push_back(h);}}
 auto measured=Ms(start);Sample(e,dt*1000,measured);
 hudTime_+=dt;if(!benchmark_&&hudTime_>=.5){Hud(e,dt);hudTime_=0;}
 if(!benchmark_){float aspect=16.f/9;if(auto* w=e.MainWindow();w&&w->Height()>0)aspect=float(w->Width())/w->Height();hud_.Place(e.Graphics(),position_,yaw_,pitch_,aspect);}
}
void PhysicsLab::Stop(Engine& e){FinishBenchmark();keys_.Reset();if(auto* window=e.MainWindow())window->SetCursorCaptured(false);Unload(e);}
