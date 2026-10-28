#pragma once

#include <PlagC/Assets/Asset.h>
#include <string>
#include <unordered_map>

namespace PlagC
{
struct AnimationRetargetConfig
{
    // Non-root-motion libraries normally leave root translation unchanged. Keeping
    // these enabled is useful for authored vertical/root offsets when the rigs match.
    bool rootTranslation = true;
    bool pelvisTranslation = true;
    // Zero derives a uniform ratio when a matching pelvis is available.
    float translationScale = 0.0f;

    // Optional target-joint -> source-joint aliases for humanoid rigs whose naming
    // conventions differ (for example 3ds Max Biped -> Unreal mannequin naming).
    // Exact name matching is still used when an alias is not supplied.
    std::unordered_map<std::string, std::string> jointMap;

    // Some authored characters are bound in a gameplay/idle pose instead of a
    // neutral retarget pose. When enabled, mapped target joints first adopt the
    // source reference-pose orientation and translation direction while preserving
    // the target bone lengths and target scales. Existing inverse-bind matrices are
    // intentionally kept, so the skinned mesh is reposed from its authored bind into
    // the neutral reference before source animation deltas are applied.
    bool adoptSourceReferencePose = false;
};

// Creates an immutable runtime model containing the target mesh/material/skeleton
// and clips retargeted from source. Unmatched target joints remain in target bind pose.
AssetHandle<ModelAsset> RetargetAnimations(AssetHandle<ModelAsset> target,
                                           AssetHandle<ModelAsset> source,
                                           const AnimationRetargetConfig &config = {});
} // namespace PlagC
