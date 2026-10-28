#include "pgcpch.h"
#include <PlagC/Animation/AnimationRetarget.h>
#include <algorithm>
#include <cmath>
#include <optional>
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
        return {0, 0, 0, 1};
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
    return name == "root" || name == "Root" || name == "ROOT" || name == "_rootJoint" ||
           name == "Bip001_01";
}

bool IsPelvis(std::string_view name)
{
    return name == "pelvis" || name == "Pelvis" || name == "hips" || name == "Hips" ||
           name.find("Pelvis") != std::string_view::npos;
}

const AnimationClip *FindClip(const ModelAsset &asset, std::string_view name)
{
    auto it = std::find_if(asset.animations.begin(), asset.animations.end(),
                           [&](const auto &clip) { return clip.name == name; });
    return it == asset.animations.end() ? nullptr : &*it;
}

JointTransform ReferenceTransform(const ModelAsset &source,
                                  const AnimationClip *reference,
                                  unsigned joint)
{
    JointTransform result = source.skeleton.joints.at(joint).local;
    if (!reference || joint >= reference->tracks.size())
        return result;

    const auto &track = reference->tracks[joint];
    if (!track.translations.empty())
        result.translation = track.translations.front().value;
    if (!track.rotations.empty())
        result.rotation = Normalize(track.rotations.front().value);
    if (!track.scales.empty())
        result.scale = track.scales.front().value;
    return result;
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

    auto resolveSource = [&](const std::string &targetName) -> std::optional<unsigned> {
        if (auto alias = config.jointMap.find(targetName); alias != config.jointMap.end())
        {
            if (auto found = sourceJoint.find(alias->second); found != sourceJoint.end())
                return found->second;
        }
        if (auto found = sourceJoint.find(targetName); found != sourceJoint.end())
            return found->second;
        return std::nullopt;
    };

    const AnimationClip *sourceReference = nullptr;
    std::string sourceReferenceName = config.sourceReferenceClip;

    if (!sourceReferenceName.empty())
    {
        sourceReference = FindClip(*source, sourceReferenceName);
        if (!sourceReference)
            throw std::invalid_argument("source reference animation clip missing: " +
                                        sourceReferenceName);
    }
    else if (!config.jointMap.empty())
    {
        // UAL and similar humanoid libraries carry an explicit neutral reference clip.
        // Prefer it for cross-rig retargeting instead of treating the imported source
        // skeleton node transforms as the animation reference pose.
        if (auto *neutral = FindClip(*source, "A_TPose"))
        {
            sourceReference = neutral;
            sourceReferenceName = "A_TPose";
        }
    }

    float translationScale = config.translationScale;
    if (translationScale <= 0)
    {
        for (const auto &targetJoint : target->skeleton.joints)
            if (IsPelvis(targetJoint.name))
            {
                if (auto mapped = resolveSource(targetJoint.name))
                {
                    const JointTransform sourceRef = ReferenceTransform(*source, sourceReference, *mapped);
                    const float s = Length(sourceRef.translation);
                    const float t = Length(targetJoint.local.translation);
                    if (s > 1e-5f && t > 1e-5f)
                        translationScale = t / s;
                }
                break;
            }
    }
    if (translationScale <= 0 || !std::isfinite(translationScale))
        translationScale = 1.0f;

    // Clone Joker exactly. The target model's own authored A/T reference pose,
    // skeleton axes, mesh, weights and inverse-bind matrices remain untouched.
    auto result = std::make_shared<ModelAsset>(*target.Shared());
    result->animations.clear();
    result->animations.reserve(source->animations.size());

    unsigned mappedCount = 0;
    for (const auto &targetJoint : target->skeleton.joints)
        if (resolveSource(targetJoint.name))
            ++mappedCount;

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
            auto sourceIndex = resolveSource(targetJoint.name);

            if (!sourceIndex)
            {
                destination.translations.push_back({0, targetJoint.local.translation});
                destination.rotations.push_back({0, targetJoint.local.rotation});
                destination.scales.push_back({0, targetJoint.local.scale});
                continue;
            }

            const auto &sourceTrack = sourceClip.tracks[*sourceIndex];
            const JointTransform sourceRef = ReferenceTransform(*source, sourceReference, *sourceIndex);
            const bool translate = (config.rootTranslation && IsRoot(targetJoint.name)) ||
                                   (config.pelvisTranslation && IsPelvis(targetJoint.name));

            if (translate && !sourceTrack.translations.empty())
            {
                for (const auto &key : sourceTrack.translations)
                {
                    const Vec3 delta{(key.value.x - sourceRef.translation.x) * translationScale,
                                     (key.value.y - sourceRef.translation.y) * translationScale,
                                     (key.value.z - sourceRef.translation.z) * translationScale};
                    destination.translations.push_back(
                        {key.time, {targetJoint.local.translation.x + delta.x,
                                    targetJoint.local.translation.y + delta.y,
                                    targetJoint.local.translation.z + delta.z}});
                }
            }
            else
            {
                destination.translations.push_back({0, targetJoint.local.translation});
            }

            const Quaternion sourceReferenceInverse = Conjugate(Normalize(sourceRef.rotation));
            if (!sourceTrack.rotations.empty())
            {
                for (const auto &key : sourceTrack.rotations)
                {
                    // UAL motion delta = inverse(UAL A_TPose) * animated UAL pose.
                    const Quaternion sourceDelta =
                        MultiplyQ(sourceReferenceInverse, Normalize(key.value));

                    // Apply that delta onto Joker's own authored A/T reference pose.
                    const Quaternion retargeted =
                        MultiplyQ(targetJoint.local.rotation, sourceDelta);
                    destination.rotations.push_back({key.time, retargeted});
                }
            }
            else
            {
                destination.rotations.push_back({0, targetJoint.local.rotation});
            }

            destination.scales.push_back({0, targetJoint.local.scale});
        }

        result->animations.push_back(std::move(clip));
    }

    result->metadata.path = target->metadata.path + "#retarget:" + source->metadata.path;
    result->metadata.generation = std::max(target->metadata.generation, source->metadata.generation);
    result->warnings.push_back("Animations retargeted at runtime from " + source->metadata.path +
                               " using " + std::to_string(mappedCount) + " mapped joints" +
                               (sourceReferenceName.empty()
                                    ? " with source bind reference"
                                    : " using source reference clip " + sourceReferenceName) +
                               "; target authored A/T reference pose preserved");
    Validate(*result);
    return AssetHandle<ModelAsset>(std::move(result));
}
} // namespace PlagC
