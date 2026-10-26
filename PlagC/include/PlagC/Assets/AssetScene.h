#pragma once
#include <PlagC/World/World.h>
namespace PlagC
{
struct AssetSceneStats
{
    std::size_t instances = 0, models = 0, animated = 0;
    double animationMs = 0, uploadMs = 0;
};
// Renderer must outlive this bridge. All resource creation/release runs on the main thread.
// Shared static meshes/materials/textures are cached per immutable model snapshot.
class AssetScene
{
  public:
    explicit AssetScene(Renderer &);
    ~AssetScene();
    AssetScene(const AssetScene &) = delete;
    AssetScene &operator=(const AssetScene &) = delete;
    void Update(World &, double dt);
    // Current animated node point in world space; empty until the instance is updated.
    std::optional<Vec3> AnimatedNodePoint(const World &, Entity, const std::string &,
                                          Vec3 local = {}) const;
    void Clear();
    AssetSceneStats Statistics() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace PlagC
