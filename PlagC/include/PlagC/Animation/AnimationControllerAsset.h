#pragma once

#include <PlagC/Animation/AnimationController.h>
#include <filesystem>
#include <vector>

namespace PlagC {

struct AnimationNodeLayout
{
    float x = 0.0f;
    float y = 0.0f;
};

struct AnimationControllerAsset
{
    AnimationStateMachine graph;
    std::vector<AnimationNodeLayout> layout;

    void Validate(unsigned clips) const;
};

void SaveAnimationControllerAsset(const std::filesystem::path& path,
                                  const AnimationControllerAsset& asset,
                                  unsigned clips);
AnimationControllerAsset LoadAnimationControllerAsset(const std::filesystem::path& path,
                                                      unsigned clips);

} // namespace PlagC
