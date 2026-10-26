#include "pgcpch.h"
#include <PlagC/World/SceneRenderer.h>
#include <PlagC/World/TransformSystem.h>
using namespace PlagC;
namespace {
void Check(bool value,const char* what) {if(!value)throw std::runtime_error(what);std::cout<<"[PASS] "<<what<<'\n';}
}
void TestSceneSynchronization(Renderer& r) {
    const auto initial=r.Statistics();
    auto mesh=r.CreateMesh(MakePrimitive(Primitive::Cube));auto material=r.CreateMaterial({});
    {
        World w;SceneRenderer scene(r),second(r);scene.SetProfiling(true);
        auto e=w.Create("Retained");w.SetRenderable(e,{mesh,material});
        scene.Sync(w);Check(scene.Statistics().created==1&&scene.Statistics().structuralRebuild,"first sync creates retained binding");
        scene.Sync(w);auto s=scene.Statistics();
        Check(!s.structuralRebuild&&!s.transforms&&!s.propertyCalls&&!s.created,"static sync performs no object renderer calls");
        second.Sync(w);w.Rename(e,"No render change");scene.Sync(w);
        Check(!scene.Statistics().transforms&&!scene.Statistics().structuralRebuild,"renaming does not dirty render extraction");
        auto t=w.Read(e).transform;t.position.x=2;w.SetTransform(e,t);scene.Sync(w);second.Sync(w);
        Check(scene.Statistics().transforms==1&&!scene.Statistics().propertyCalls&&second.Statistics().transforms==1,"transform dirty state reaches independent bridges exactly once");
        w.SetRenderable(e,{mesh,material,false});scene.Sync(w);
        Check(scene.Statistics().propertyCalls==1&&!scene.Statistics().transforms&&!scene.Statistics().structuralRebuild,"visibility update changes only one renderer property");
        TransformSystem system;const std::array selected{e};system.Bind(w,selected);system.Prepare()[0].position.x=3;system.Commit();scene.Sync(w);
        Check(scene.Statistics().transforms==1&&!scene.Statistics().propertyCalls,"system commit marks transform revision for rendering");
        auto invalid=r.CreateMaterial({});r.DestroyMaterial(invalid);w.SetRenderable(e,{mesh,invalid});
        bool rejected=false;try{scene.Sync(w);}catch(const std::exception&){rejected=true;}
        Check(rejected&&scene.Objects()==1,"failed resource replacement keeps previous object alive");
        w.SetRenderable(e,{mesh,material});scene.Sync(w);
        w.RemoveRenderable(e);w.SetRenderable(e,{mesh,material,false,false,20});scene.Sync(w);
        Check(scene.Objects()==1&&scene.Statistics().propertyCalls==3,"remove/readd before sync refreshes all changed flags");
        w.SetLight(e,{});w.SetCamera(e,{{0,0,0}});w.SetActiveCamera(e);scene.Sync(w);scene.Sync(w);
        Check(!scene.Statistics().lightCalls&&!scene.Statistics().cameraCalls,"unchanged lights and camera skip renderer calls");
        t.position.x=4;w.SetTransform(e,t);scene.Sync(w);
        Check(scene.Statistics().transforms==1&&scene.Statistics().lightCalls==1&&scene.Statistics().cameraCalls==1,"one transform updates object light and active camera");
        w.RemoveRenderable(e);scene.Sync(w);Check(!scene.Objects(),"structural removal destroys retained object");
        w.Destroy(e);scene.Sync(w);Check(!scene.Lights(),"entity destruction removes retained light");
        w.Clear();scene.Sync(w);
        World other;auto x=other.Create();other.SetRenderable(x,{mesh,material});scene.Sync(other);
        Check(scene.Objects()==1,"bridge switches worlds with independent revisions");
        scene.Clear();scene.Sync(other);Check(scene.Objects()==1,"explicit Clear rebuilds next sync");
    }
    r.DestroyMaterial(material);r.DestroyMesh(mesh);const auto final=r.Statistics();
    Check(final.objects==initial.objects&&final.lights==initial.lights&&final.meshes==initial.meshes&&final.materials==initial.materials,"retained scene tests release all resources");
}
