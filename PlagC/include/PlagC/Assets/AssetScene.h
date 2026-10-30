#pragma once
#include <PlagC/World/World.h>
namespace PlagC
{
struct AssetSceneStats
{
    std::size_t instances = 0, models = 0, animated = 0;
    // poseMs covers controller evaluation, Ozz sampling/blending and local-to-model.
    // skinningMs is CPU vertex deformation; uploadMs is dynamic mesh upload to the renderer.
    double animationMs = 0, poseMs = 0, skinningMs = 0, uploadMs = 0;
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
    // animationOnly restricts positive-delta animation to one entity. presentationOnly samples
    // editor-owned copies so Scene View preview never mutates authoring AnimatorComponent state.
    void Update(World &, double dt, Entity animationOnly = {}, bool presentationOnly = false);
    std::optional<Vec3> AnimatedNodePoint(const World &, Entity, const std::string &,
                                          Vec3 local = {}) const;
    // Owner-thread identity bridge for editor/tooling picking. Objects not owned by a
    // ModelComponent instance resolve to an empty Entity.
    Entity EntityForObject(ObjectHandle) const;
    void Clear();
    AssetSceneStats Statistics() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace PlagC
