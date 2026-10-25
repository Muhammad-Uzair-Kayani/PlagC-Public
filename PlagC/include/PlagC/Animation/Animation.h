#pragma once
#include <PlagC/Assets/Asset.h>
namespace PlagC
{
struct AnimationState
{
    unsigned clip = 0;
    double time = 0;
    float speed = 1;
    bool playing = true, loop = true;
};
struct AnimatorComponent
{
    AnimationState state;
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
    std::span<const Matrix4> Pose() const;
    MeshData Deform(
        unsigned mesh) const; // CPU skinning baseline; GPU skinning can replace this adapter.
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace PlagC
