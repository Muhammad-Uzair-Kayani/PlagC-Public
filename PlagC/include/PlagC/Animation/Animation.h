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
    MeshData Deform(
        unsigned mesh) const; // CPU skinning baseline; GPU skinning can replace this adapter.
  private:
    void ApplyUpperBody(AnimatorComponent &);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace PlagC
