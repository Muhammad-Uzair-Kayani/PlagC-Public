#include "pgcpch.h"
#include <PlagC/Animation/AnimationRetarget.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace PlagC
{
namespace
{
Quaternion Conjugate(Quaternion q)
{
    return {-q.x, -q.y, -q.z, q.w};
}

Quaternion Normalize(Quaternion q)
{
    const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (n <= 1e-8f)
        return {};
    return {q.x / n, q.y / n, q.z / n, q.w / n};
}

Quaternion MultiplyQ(Quaternion a, Quaternion b)
{
    return Normalize({a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                      a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                      a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z});
}

float Length(Vec3 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

bool IsRoot(std::string_view name)
{
    return name == "root" || name == "Root" || name == "ROOT";
}

bool IsPelvis(std::string_view name)
{
    return name == "pelvis" || name == "Pelvis" || name == "hips" || name == "Hips";
}
} // namespace

AssetHandle<ModelAsset> RetargetAnimations(AssetHandle<ModelAsset> target,
                                           AssetHandle<ModelAsset> source,
                                           const AnimationRetargetConfig &config)
{
    if (!target || !source)
        throw std::invalid_argument("retarget requires target and source model assets");
    Validate(*target);
    Validate(*source);
    if (source->animations.empty())
        throw std::invalid_argument("retarget source contains no animations");

    std::unordered_map<std::string, unsigned> sourceJoint;
    sourceJoint.reserve(source->skeleton.joints.size());
    for (unsigned i = 0; i < source->skeleton.joints.size(); ++i)
        sourceJoint.emplace(source->skeleton.joints[i].name, i);

    float translationScale = config.translationScale;
    if (translationScale <= 0)
    {
        auto targetPelvis = std::find_if(target->skeleton.joints.begin(), target->skeleton.joints.end(),
                                         [](const auto &j) { return IsPelvis(j.name); });
        if (targetPelvis != target->skeleton.joints.end())
        {
            auto found = sourceJoint.find(targetPelvis->name);
            if (found != sourceJoint.end())
            {
                const float s = Length(source->skeleton.joints[found->second].local.translation);
                const float t = Length(targetPelvis->local.translation);
                if (s > 1e-5f && t > 1e-5f)
                    translationScale = t / s;
            }
        }
    }
    if (translationScale <= 0 || !std::isfinite(translationScale))
        translationScale = 1.0f;

    auto result = std::make_shared<ModelAsset>(*target.Shared());
    result->animations.clear();
    result->animations.reserve(source->animations.size());

    for (const auto &sourceClip : source->animations)
    {
        AnimationClip clip;
        clip.name = sourceClip.name;
        clip.duration = sourceClip.duration;
        clip.tracks.resize(target->skeleton.joints.size());

        for (unsigned targetIndex = 0; targetIndex < target->skeleton.joints.size(); ++targetIndex)
        {
            const auto &targetJoint = target->skeleton.joints[targetIndex];
            auto &destination = clip.tracks[targetIndex];
            auto found = sourceJoint.find(targetJoint.name);
            if (found == sourceJoint.end())
            {
                destination.translations.push_back({0, targetJoint.local.translation});
                destination.rotations.push_back({0, targetJoint.local.rotation});
                destination.scales.push_back({0, targetJoint.local.scale});
                continue;
            }

            const unsigned sourceIndex = found->second;
            const auto &sourceJointNode = source->skeleton.joints[sourceIndex];
            const auto &sourceTrack = sourceClip.tracks[sourceIndex];
            const bool translate = (config.rootTranslation && IsRoot(targetJoint.name)) ||
                                   (config.pelvisTranslation && IsPelvis(targetJoint.name));

            if (translate)
            {
                for (const auto &key : sourceTrack.translations)
                {
                    const Vec3 delta{(key.value.x - sourceJointNode.local.translation.x) * translationScale,
                                     (key.value.y - sourceJointNode.local.translation.y) * translationScale,
                                     (key.value.z - sourceJointNode.local.translation.z) * translationScale};
                    destination.translations.push_back(
                        {key.time, {targetJoint.local.translation.x + delta.x,
                                    targetJoint.local.translation.y + delta.y,
                                    targetJoint.local.translation.z + delta.z}});
                }
            }
            else
                destination.translations.push_back({0, targetJoint.local.translation});

            const Quaternion sourceBindInverse = Conjugate(Normalize(sourceJointNode.local.rotation));
            for (const auto &key : sourceTrack.rotations)
            {
                // targetBind * inverse(sourceBind) * sourceAnimated preserves the
                // target rest orientation while applying the source local rotation delta.
                const Quaternion retargeted = MultiplyQ(
                    targetJoint.local.rotation,
                    MultiplyQ(sourceBindInverse, Normalize(key.value)));
                destination.rotations.push_back({key.time, retargeted});
            }

            // Body proportions belong to the target. Animation scale is deliberately
            // ignored because copying source scale is the most common cause of visual
            // collapse when retargeting between differently proportioned characters.
            destination.scales.push_back({0, targetJoint.local.scale});
        }

        result->animations.push_back(std::move(clip));
    }

    result->metadata.path = target->metadata.path + "#retarget:" + source->metadata.path;
    result->metadata.generation = std::max(target->metadata.generation, source->metadata.generation);
    result->warnings.push_back("Animations retargeted at runtime from " + source->metadata.path);
    Validate(*result);
    return AssetHandle<ModelAsset>(std::move(result));
}
} // namespace PlagC
