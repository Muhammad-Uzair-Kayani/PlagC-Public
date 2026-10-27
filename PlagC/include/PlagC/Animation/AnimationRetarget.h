#pragma once

#include <PlagC/Assets/Asset.h>

namespace PlagC
{
struct AnimationRetargetConfig
{
    // Non-root-motion libraries normally leave root translation unchanged. Keeping
    // these enabled is still useful for jumps/vertical pelvis motion and authored
    // root offsets; deltas are applied relative to the source bind pose.
    bool rootTranslation = true;
    bool pelvisTranslation = true;
    // Zero derives a uniform ratio from the pelvis bind translation lengths.
    float translationScale = 0.0f;
};

// Creates an immutable runtime model containing the target mesh/material/skeleton
// and clips retargeted from source. Joint matching is name based. Unmatched target
// joints remain in their target bind pose.
AssetHandle<ModelAsset> RetargetAnimations(AssetHandle<ModelAsset> target,
                                           AssetHandle<ModelAsset> source,
                                           const AnimationRetargetConfig &config = {});
} // namespace PlagC
