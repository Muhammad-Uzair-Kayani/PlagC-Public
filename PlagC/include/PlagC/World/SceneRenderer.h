#pragma once
#include <PlagC/Renderer/Renderer.h>
#include <PlagC/World/World.h>
#include <memory>
#include <span>
namespace PlagC {
struct SceneSyncStats {
    std::size_t scanned=0,created=0,destroyed=0,transforms=0,propertyCalls=0,lightCalls=0,cameraCalls=0;
    bool structuralRebuild=false,detailed=false;
    double totalMs=0,structureMs=0,resourceMs=0,transformMs=0,propertyMs=0,lightCameraMs=0,traversalMs=0;
};
class SceneRenderer {
public:
    explicit SceneRenderer(Renderer&);
    ~SceneRenderer();
    SceneRenderer(const SceneRenderer&)=delete;
    SceneRenderer& operator=(const SceneRenderer&)=delete;
    void Sync(const World&,std::span<const TransformUpdate> presentation={});
    void Clear();
    std::size_t Objects() const;
    std::size_t Lights() const;
    void SetProfiling(bool);
    SceneSyncStats Statistics() const;

    // Owner-thread identity bridge used by editor/tooling integration. Non-scene
    // renderer objects (HUD, diagnostics, etc.) resolve to an empty Entity.
    ObjectHandle ObjectFor(Entity) const;
    Entity EntityForObject(ObjectHandle) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
