#pragma once
#include <PlagC/Assets/Asset.h>
#include <PlagC/Animation/AnimationController.h>
namespace PlagC
{
struct AnimationState
{
    unsigned clip = 0;
    double time = 0;
    float speed = 1;
    bool playing = true, loop = true;
};
// Optional masked weapon pose. Root and descendants only; locomotion keeps its clock.
struct AnimationUpperBody
{
    AnimationState pose;
    std::string root;
    float weight = 0;
    float recoilRadians = 0;
};
struct AnimatorComponent
{
    // Optional external animation source. SurvivalGame currently uses a retargeted
    // runtime model instead, but the field remains for compatibility with World validation.
    AssetHandle<ModelAsset> animationSource;
    AnimationState state;
    std::optional<AnimationController> controller;
    std::optional<AnimationUpperBody> upperBody;
    AnimationState previous;
    double fadeElapsed = 0, fadeDuration = 0;
    bool controllerStarted = false;
};
// Ozz runtime state is private; one Animator per independently animated instance.
class Animator
{
  public:
    explicit Animator(AssetHandle<ModelAsset>);
    ~Animator();
    Animator(const Animator &) = delete;
    Animator &operator=(const Animator &) = delete;
    void Update(AnimationState &, double dt);
    void Update(AnimatorComponent &, double dt);
    std::span<const Matrix4> Pose() const;
    // Allocation-free hot path. Storage is owned by this Animator and remains valid until the
    // next DeformVertices call for the same mesh or Animator destruction.
    std::span<const Vertex> DeformVertices(unsigned mesh) const;
    // Convenience/setup path that retains indices and other MeshData fields. Per-frame animation
    // should use DeformVertices so static mesh data is not copied every frame.
    MeshData Deform(unsigned mesh) const;
  private:
    void ApplyUpperBody(AnimatorComponent &, double dt);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace PlagC
