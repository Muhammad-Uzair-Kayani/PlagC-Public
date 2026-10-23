#pragma once
#include <PlagC/World/World.h>
namespace PlagC
{
struct SceneSyncStats {
    std::size_t scanned=0, transforms=0, propertyCalls=0, lightCalls=0, cameraCalls=0, created=0, destroyed=0;
    bool structuralRebuild=false, detailed=false;
    double totalMs=0, structureMs=0, resourceMs=0, transformMs=0, propertyMs=0, lightCameraMs=0, traversalMs=0;
};
// Owns renderer instances, borrows mesh/material handles. Renderer must outlive this bridge.
// Sync/Clear/destruction are main-thread only. World can be destroyed independently.
class SceneRenderer
{
  public:
    explicit SceneRenderer(Renderer &);
    ~SceneRenderer();
    SceneRenderer(const SceneRenderer &) = delete;
    SceneRenderer &operator=(const SceneRenderer &) = delete;
    void Sync(const World &);
    void Clear();
    void SetProfiling(bool); // Per-call clocks are opt-in; counters always enabled.
    SceneSyncStats Statistics() const;
    std::size_t Objects() const;
    std::size_t Lights() const;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}

