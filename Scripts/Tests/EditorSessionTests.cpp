#include <PlagC/Editor/EditorSession.h>
#include <PlagC/Thread.h>
#include <cassert>
#include <PlagC/Camera/CameraSystem.h>
#include <PlagC/World/LightSystem.h>
#include <chrono>
#include <filesystem>
#include <fstream>
using namespace PlagC;
using namespace PlagC::Editor;

void TestCameraPose() {
    auto world = std::make_unique<World>();
    const auto parent = world->Create("Parent");
    const auto camera = world->Create("Camera");
    Transform pose; pose.position={2,3,4};
    world->SetTransform(camera,pose);
    world->SetCamera(camera,{});
    world->SetParent(camera,parent);
    world->SetActiveCamera(camera);
    SceneDocument document(std::move(world));
    CameraSystem system; system.SetViewport(1600,900);
    auto near=[](Vec3 a,Vec3 b) {
        return std::abs(a.x-b.x)<0.001f && std::abs(a.y-b.y)<0.001f && std::abs(a.z-b.z)<0.001f;
    };
    auto forward=[](CameraDesc c) {return Vec3{c.target.x-c.position.x,c.target.y-c.position.y,c.target.z-c.position.z};};
    assert(near(forward(system.Extract(document.Scene())),{0,0,-1}));
    pose.rotationDegrees={0,90,90};
    document.SetTransform(camera,pose);
    auto extracted=system.Extract(document.Scene());
    assert(near(forward(extracted),{-1,0,0}));
    assert(near(extracted.up,{0,0,1}));
    assert(near(RotateDirection(DirectionRotation(forward(extracted),extracted.up),{0,1,0}),extracted.up));
    assert(document.Undo());
    assert(near(forward(system.Extract(document.Scene())),{0,0,-1}));
    assert(document.Redo());
    const auto saved=document.Serialize();
    auto loaded=SceneDocument::Deserialize(saved);
    assert(near(forward(system.Extract(loaded->Scene())),forward(extracted)));
    assert(near(system.Extract(loaded->Scene()).up,extracted.up));
    EditorSession session(std::move(loaded));
    session.StartPlay();
    assert(near(forward(system.Extract(session.ViewWorld())),forward(extracted)));
    session.PausePlay(); session.RequestStep(); assert(session.ConsumeStep()); session.StopPlay();
    assert(session.Document().Serialize()==saved);

    const auto legacy=R"({"format":"PlagC.Scene","version":4,"activeCamera":"2","entities":[
      {"id":"1","parent":null,"name":"Parent","transform":{"position":[0,0,0],"rotation":[0,35,0],"scale":[1,1,1]}},
      {"id":"2","parent":"1","name":"Camera","transform":{"position":[2,3,4],"rotation":[0,0,0],"scale":[1,1,1]},"camera":{"target":[3,3,4],"fov":60,"near":0.1,"far":1000,"aspect":0}}
    ]})";
    auto migrated=SceneDocument::Deserialize(legacy);
    const auto oldView=system.Extract(migrated->Scene());
    assert(near(oldView.position,{2,3,4}));
    assert(near(forward(oldView),{1,0,0}));
    auto migratedAgain=SceneDocument::Deserialize(migrated->Serialize());
    assert(near(forward(system.Extract(migratedAgain->Scene())),{1,0,0}));
}

void TestLightPose() {
    auto world=std::make_unique<World>();
    const auto parent=world->Create("Parent");
    const auto light=world->Create("Spot");
    Transform pose; pose.position={1,0,0};
    world->SetTransform(light,pose);
    LightComponent component; component.light.type=LightType::Spot;
    world->SetLight(light,component); world->SetParent(light,parent);
    SceneDocument document(std::move(world));
    auto near=[](Vec3 a,Vec3 b) {return std::abs(a.x-b.x)<0.001f && std::abs(a.y-b.y)<0.001f && std::abs(a.z-b.z)<0.001f;};
    auto extracted=[&] {const auto r=document.Scene().Read(light); return ExtractLight(*r.light,r.transform);};
    assert(near(extracted().direction,{0,0,-1}));
    Transform parentPose; parentPose.rotationDegrees.y=90;
    document.SetTransform(parent,parentPose);
    assert(near(extracted().direction,{-1,0,0}));
    assert(near(extracted().position,{0,0,-1}));
    assert(document.Undo()); assert(near(extracted().direction,{0,0,-1}));
    assert(document.Redo());
    auto loaded=SceneDocument::Deserialize(document.Serialize());
    const auto record=loaded->Scene().Read(loaded->FindByPersistentID(document.PersistentID(light)));
    assert(near(ExtractLight(*record.light,record.transform).direction,{-1,0,0}));

    const auto legacy=R"({"format":"PlagC.Scene","version":5,"activeCamera":null,"entities":[
      {"id":"1","parent":null,"name":"Directional","transform":{"position":[2,3,4],"rotation":[0,0,0],"scale":[1,1,1]},"light":{"type":0,"position":[0,0,0],"direction":[1,-1,0],"colour":[1,1,1],"power":1,"radius":10,"inner":20,"outer":30}}
    ]})";
    auto migrated=SceneDocument::Deserialize(legacy);
    const auto migratedRecord=migrated->Scene().Read(migrated->FindByPersistentID(1));
    const auto desc=ExtractLight(*migratedRecord.light,migratedRecord.transform);
    assert(near(desc.position,{2,3,4}));
    assert(near(desc.direction,{0.7071068f,-0.7071068f,0}));
    auto reloaded=SceneDocument::Deserialize(migrated->Serialize());
    const auto r=reloaded->Scene().Read(reloaded->FindByPersistentID(1));
    assert(near(ExtractLight(*r.light,r.transform).direction,desc.direction));
    std::string conflicting=legacy;
    const auto marker=conflicting.find("\"light\":");
    conflicting.insert(marker,"\"camera\":{\"fov\":60,\"near\":0.1,\"far\":1000,\"aspect\":0},");
    bool rejected=false;
    try { (void)SceneDocument::Deserialize(conflicting); }
    catch(const std::exception& e) { rejected=std::string(e.what()).find("conflicting camera/light")!=std::string::npos; }
    assert(rejected);
}

int main() {
    Thread::InitializeMain();
    TestCameraPose();
    TestLightPose();

    auto world = std::make_unique<World>();
    auto entity = world->Create("Authoring");
    auto document = std::make_unique<SceneDocument>(std::move(world));
    const auto id = document->PersistentID(entity);
    EditorSession session(std::move(document));

    assert(!session.HasPath() && session.EditingEnabled());
    assert(session.ViewMode() == EditorViewMode::Scene);
    session.EditDocument().Rename(entity, "Dirty");
    assert(session.Dirty());

    bool dirtyRejected = false;
    try { session.NewEmpty(); }
    catch (const std::logic_error&) { dirtyRejected = true; }
    assert(dirtyRejected);
    assert(session.Document().FindByPersistentID(id));

    const auto root = std::filesystem::temp_directory_path() /
        ("plagc-editor-session-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto firstPath = root / "first.plscene";
    const auto secondPath = root / "second.plscene";
    const auto brokenPath = root / "broken.plscene";

    session.SaveAs(firstPath);
    assert(session.HasPath() && session.Path() == firstPath && !session.Dirty());

    session.EditDocument().Rename(session.Document().FindByPersistentID(id), "PlaySource");
    assert(session.Dirty());
    session.SelectViewPersistent(id);
    const auto authoringBeforePlay = session.Document().Serialize();
    const auto authoringRevision = session.Document().Revision();
    const bool authoringUndo = session.Document().CanUndo();
    session.StartPlay();
    assert(!session.ViewDocument().Selection());
    const bool runtimeDirty = session.RuntimeDocument().Dirty();
    const bool runtimeUndo = session.RuntimeDocument().CanUndo();
    session.SelectViewPersistent(id);
    assert(session.ViewDocument().Selection() == session.RuntimeDocument().FindByPersistentID(id));
    assert(session.ViewDocument().Snapshot().front().selected);
    bool selectionRejected = false;
    try { session.SelectViewPersistent(id + 1000); }
    catch (const std::invalid_argument&) { selectionRejected = true; }
    assert(selectionRejected && session.ViewDocument().Selection());
    assert(session.RuntimeDocument().Dirty() == runtimeDirty);
    assert(session.RuntimeDocument().CanUndo() == runtimeUndo);
    assert(session.Document().Revision() == authoringRevision);
    assert(session.Document().Selection() == entity);
    assert(session.Playing() && &session.ViewWorld() == &session.RuntimeWorld());
    assert(session.ViewDocument().FindByPersistentID(id));

    bool editRejected = false;
    try { (void)session.EditDocument(); }
    catch (const std::logic_error&) { editRejected = true; }
    assert(editRejected && !session.CanUndo());

    const auto runtimeEntity = session.RuntimeDocument().FindByPersistentID(id);
    assert(runtimeEntity);
    session.RuntimeWorld().Rename(runtimeEntity, "RuntimeOnly");
    assert(session.Document().Scene().Read(session.Document().FindByPersistentID(id)).name == "PlaySource");
    assert(session.RuntimeWorld().Read(runtimeEntity).name == "RuntimeOnly");

    assert(session.ViewMode() == EditorViewMode::Game);
    session.PausePlay();
    session.SelectViewPersistent(0);
    assert(!session.ViewDocument().Selection());
    session.SelectViewPersistent(id);
    assert(session.ViewDocument().Selection() == runtimeEntity);
    assert(session.ViewMode() == EditorViewMode::Game);
    session.RequestStep();
    assert(session.ConsumeStep());
    assert(session.ViewMode() == EditorViewMode::Game);
    session.StopPlay();
    assert(!session.Playing() && session.EditingEnabled());
    assert(session.ViewMode() == EditorViewMode::Scene);

    assert(session.Document().Serialize() == authoringBeforePlay);
    assert(session.Document().Revision() == authoringRevision);
    assert(session.Document().Selection() == entity);
    assert(session.Document().CanUndo() == authoringUndo);
    session.StartPlay();
    assert(!session.ViewDocument().Selection());
    session.StopPlay();

    session.Save();
    assert(!session.Dirty());

    // A failed Open must preserve both the live document and its current path.
    std::ofstream(brokenPath) << "{not-json";
    bool openFailed = false;
    try { session.Open(brokenPath, {}, ReplacePolicy::DiscardChanges); }
    catch (const std::exception&) { openFailed = true; }
    assert(openFailed && session.Path() == firstPath);
    assert(session.Document().FindByPersistentID(id));

    auto otherWorld = std::make_unique<World>();
    otherWorld->Create("Second");
    auto other = std::make_unique<SceneDocument>(std::move(otherWorld));
    other->Save(secondPath);

    session.Open(secondPath, {}, ReplacePolicy::DiscardChanges);
    assert(session.Path() == secondPath && !session.Dirty());
    const auto snapshot = session.Document().Snapshot();
    assert(snapshot.size() == 1 && snapshot.front().record.name == "Second");

    session.NewEmpty(ReplacePolicy::DiscardChanges);
    assert(!session.HasPath() && session.Document().Scene().Size() == 0);

    std::filesystem::remove_all(root);
}
