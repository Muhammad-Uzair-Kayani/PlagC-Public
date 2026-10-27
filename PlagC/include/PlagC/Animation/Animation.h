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
struct AnimatorTimings
{
    double stateMachineMs = 0;
    double samplingMs = 0;
    double localToModelMs = 0;
    double blendingMs = 0;
    double upperBodyMs = 0;
    double skinningMs = 0;
};
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
    MeshData Deform(unsigned mesh) const;
    AnimatorTimings ConsumeTimings();
  private:
    void ApplyUpperBody(AnimatorComponent &, double dt);
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace PlagC
