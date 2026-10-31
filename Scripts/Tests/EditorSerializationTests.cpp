#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/Thread.h>
#include <cassert>
#include <fstream>
#include <chrono>
using namespace PlagC;
using PlagC::Editor::SceneDocument;
template<class F> void Reject(F f) { bool failed=false; try { f(); } catch(const std::exception&) { failed=true; } assert(failed); }
int main() {
    Thread::InitializeMain();
    auto world=std::make_unique<World>();
    auto camera=world->Create("Camera"); Transform t; t.position={2,3,4}; world->SetTransform(camera,t);
    world->SetCamera(camera,{}); world->SetActiveCamera(camera);
    auto light=world->Create("Light"); world->SetLight(light,{});
    auto plain=world->Create(std::string("Name\0with \"quotes\"",18));
    auto asset=std::make_shared<ModelAsset>(); asset->metadata.path="Game://props/crate.plasset";
    asset->animations.push_back({"Idle",1.0f,{}});
    auto model=world->Create("Crate"); world->SetModel(model,{AssetHandle<ModelAsset>(asset),false});
    world->SetParent(model,plain);

    PhysicsComponent physics;
    physics.body.motion=MotionType::Dynamic;
    physics.body.layer=CollisionLayer::Dynamic;
    physics.body.collisionMask=CollisionLayerBit(CollisionLayer::StaticWorld)|CollisionLayerBit(CollisionLayer::Dynamic);
    physics.body.mass=3.0f; physics.body.gravityScale=0.75f; physics.body.linearDamping=0.15f;
    physics.body.angularDamping=0.25f; physics.body.continuous=true; physics.body.collisionGroup=17;
    physics.collider.type=ColliderType::Box; physics.collider.dimensions={0.5f,1.0f,1.5f};
    physics.collider.material.friction=0.7f; physics.collider.material.restitution=0.2f;
    physics.collider.centerOfMassOffset={0.1f,0.2f,0.3f};
    auto physical=world->Create("Physics"); world->SetPhysics(physical,physics);

    AnimatorComponent animator;
    animator.animationSource=AssetHandle<ModelAsset>(asset);
    animator.state.clip=0; animator.state.time=0.25; animator.state.speed=1.25f; animator.state.playing=true; animator.state.loop=false;
    AnimationStateMachine graph;
    graph.parameters.push_back({"Speed",AnimationParameterType::Float,0.0f});
    graph.states.push_back({"Idle",0,true,1.0f}); graph.entry=0;
    animator.controller.emplace(graph,1); animator.controller->SetFloat("Speed",0.6f);
    AnimationUpperBody upper; upper.pose.clip=0; upper.pose.time=0.1; upper.root="Spine"; upper.weight=0.35f; upper.recoilRadians=0.05f;
    animator.upperBody=upper;
    world->SetAnimator(model,animator);

    SceneDocument d(std::move(world));
    auto text=d.Serialize();
    assert(text.find("\"version\":4")!=std::string::npos);
    assert(text.find("\"castShadows\":true")!=std::string::npos);
    assert(text.find("\"materials\":[]")!=std::string::npos);
    assert(text.find("\"physics\":")!=std::string::npos);
    assert(text.find("\"animator\":")!=std::string::npos);
    int loads=0;
    auto resolver=[&](const std::string &path) { ++loads; assert(path==asset->metadata.path); return AssetHandle<ModelAsset>(asset); };
    auto restored=SceneDocument::Deserialize(text,resolver);
    assert(loads==2 && !restored->Dirty() && !restored->Selection());
    assert(restored->Serialize()==text);
    assert(restored->Scene().ActiveCamera()!=camera);
    assert(restored->PersistentID(restored->Scene().ActiveCamera())==d.PersistentID(camera));
    assert(restored->Scene().Size()==5);
    auto restoredModel=restored->FindByPersistentID(d.PersistentID(model));
    auto restoredPlain=restored->FindByPersistentID(d.PersistentID(plain));
    auto restoredPhysical=restored->FindByPersistentID(d.PersistentID(physical));
    assert(restored->Scene().Parent(restoredModel)==restoredPlain);
    const auto restoredPhysics=restored->Scene().Read(restoredPhysical).physics;
    assert(restoredPhysics && restoredPhysics->body.mass==3.0f && restoredPhysics->body.collisionGroup==17);
    const auto restoredAnimator=restored->Scene().Read(restoredModel).animator;
    assert(restoredAnimator && restoredAnimator->controller && restoredAnimator->animationSource);
    assert(restoredAnimator->controller->Current().name=="Idle");
    assert(restoredAnimator->controller->Value("Speed")==0.6f);
    const auto createdAfterLoad=restored->Create();
    assert(restored->PersistentID(createdAfterLoad)>d.PersistentID(physical));
    Reject([&]{SceneDocument::Deserialize(text);});
    Reject([&]{SceneDocument::Deserialize(text,[](const std::string&){return AssetHandle<ModelAsset>{};});});
    auto replace=[&](std::string from,std::string to) {auto s=text; auto p=s.find(from); assert(p!=std::string::npos); s.replace(p,from.size(),to); return s;};
    Reject([&]{SceneDocument::Deserialize(replace("\"version\":4","\"version\":5"),resolver);});
    Reject([&]{SceneDocument::Deserialize(replace("\"version\":4","\"version\":4,\"version\":4"),resolver);});
    Reject([&]{SceneDocument::Deserialize(replace("\"id\":\"2\"","\"id\":\"1\""),resolver);});
    Reject([&]{SceneDocument::Deserialize(replace("\"activeCamera\":\"1\"","\"activeCamera\":\"999\""),resolver);});
    Reject([&]{SceneDocument::Deserialize(replace("\"activeCamera\":\"1\"","\"activeCamera\":\"2\""),resolver);});
    Reject([&]{SceneDocument::Deserialize(replace("\"scale\":[1.0,1.0,1.0]","\"scale\":[-1,1,1]"),resolver);});
    Reject([&]{SceneDocument::Deserialize(replace("\"parent\":null","\"parent\":\"999\""),resolver);});
    Reject([&]{SceneDocument::Deserialize(replace("Game://props/crate.plasset","../crate.plasset"),resolver);});
    Reject([&]{SceneDocument::Deserialize("{}");});
    Reject([&]{SceneDocument::Deserialize(text+"garbage",resolver);});
    Reject([&]{SceneDocument::Deserialize(text+std::string("\0garbage",8),resolver);});
    auto unsupported=std::make_unique<World>(); auto e=unsupported->Create();
    unsupported->SetRenderable(e,{{1},{2}}); SceneDocument unsupportedDoc(std::move(unsupported));
    Reject([&]{unsupportedDoc.Serialize();});
    auto root=std::filesystem::temp_directory_path()/("plagc-scene-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(std::filesystem::create_directory(root));
    auto path=root/"scene.json";
    d.Save(path); d.Rename(plain,"Renamed"); assert(d.Dirty());
    Reject([&]{d.Save(root/"missing"/"scene.json");}); assert(d.Dirty());
    assert(SceneDocument::Load(path,resolver)->Serialize()==text);
    Reject([&]{unsupportedDoc.Save(path);});
    assert(SceneDocument::Load(path,resolver)->Serialize()==text);
    d.Save(path); assert(!d.Dirty());
    assert(SceneDocument::Load(path,resolver)->Serialize()==d.Serialize());
    d.Rename(plain,std::string(1,static_cast<char>(0xff)));
    Reject([&]{d.Save(path);}); assert(d.Dirty());
    for(const auto &entry:std::filesystem::directory_iterator(root)) assert(entry.path()==path);
    std::filesystem::remove_all(root);
}
