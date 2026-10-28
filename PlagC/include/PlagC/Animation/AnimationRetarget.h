#pragma once

#include <PlagC/Assets/Asset.h>
#include <string>
#include <unordered_map>

namespace PlagC
{
struct AnimationRetargetConfig
{
    bool rootTranslation = true;
    bool pelvisTranslation = true;
    // Zero derives a uniform ratio when a matching pelvis is available.
    float translationScale = 0.0f;

    // Optional target-joint -> source-joint aliases for rigs with different names.
    std::unordered_map<std::string, std::string> jointMap;

    // Optional source neutral/reference clip. When set, animation deltas are measured
    // from this clip instead of from the source skeleton's raw bind transforms.
    // The target reference is always the target model's own authored skeleton pose.
    std::string sourceReferenceClip;

    // Keep target-native clips before appending retargeted source clips. This lets a
    // character use its own authored idle while borrowing locomotion/actions from a
    // shared animation library on the same runtime skeleton.
    bool preserveTargetAnimations = false;
};

// Retargets source animation deltas onto the target model's own authored reference pose.
// The target skeleton, mesh, skin weights and inverse-bind matrices are not rewritten.
AssetHandle<ModelAsset> RetargetAnimations(AssetHandle<ModelAsset> target,
                                           AssetHandle<ModelAsset> source,
                                           const AnimationRetargetConfig &config = {});
} // namespace PlagC
