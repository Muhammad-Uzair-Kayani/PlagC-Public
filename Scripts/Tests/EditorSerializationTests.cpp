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
    auto model=world->Create("Crate"); world->SetModel(model,{AssetHandle<ModelAsset>(asset),false});
    SceneDocument d(std::move(world));
    auto text=d.Serialize();
    int loads=0;
    auto resolver=[&](const std::string &path) { ++loads; assert(path==asset->metadata.path); return AssetHandle<ModelAsset>(asset); };
    auto restored=SceneDocument::Deserialize(text,resolver);
    assert(loads==1 && !restored->Dirty() && !restored->Selection());
    assert(restored->Serialize()==text);
    assert(restored->Scene().ActiveCamera()!=camera);
    assert(restored->PersistentID(restored->Scene().ActiveCamera())==d.PersistentID(camera));
    assert(restored->Scene().Size()==4);
    Reject([&]{SceneDocument::Deserialize(text);});
    Reject([&]{SceneDocument::Deserialize(text,[](const std::string&){return AssetHandle<ModelAsset>{};});});
    auto replace=[&](std::string from,std::string to) {auto s=text; auto p=s.find(from); assert(p!=std::string::npos); s.replace(p,from.size(),to); return s;};
    Reject([&]{SceneDocument::Deserialize(replace("\"version\":1","\"version\":2"),resolver);});
    Reject([&]{SceneDocument::Deserialize(replace("\"version\":1","\"version\":1,\"version\":1"),resolver);});
    Reject([&]{SceneDocument::Deserialize(replace("\"id\":\"2\"","\"id\":\"1\""),resolver);});
    Reject([&]{SceneDocument::Deserialize(replace("\"activeCamera\":\"1\"","\"activeCamera\":\"999\""),resolver);});
    Reject([&]{SceneDocument::Deserialize(replace("\"activeCamera\":\"1\"","\"activeCamera\":\"2\""),resolver);});
    Reject([&]{SceneDocument::Deserialize(replace("\"scale\":[1.0,1.0,1.0]","\"scale\":[-1,1,1]"),resolver);});
    assert(loads==1); // schema/component failures precede asset resolution
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
