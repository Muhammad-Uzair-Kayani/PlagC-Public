#include "pgcpch.h"
#include <PlagC/Animation/AnimationRetarget.h>
#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_map>
#include <vector>

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

Vec3 Rotate(Quaternion q, Vec3 v)
{
    q = Normalize(q);
    const Vec3 u{q.x, q.y, q.z};
    const float dotUV = u.x * v.x + u.y * v.y + u.z * v.z;
    const float dotUU = u.x * u.x + u.y * u.y + u.z * u.z;
    const Vec3 cross{u.y * v.z - u.z * v.y,
                     u.z * v.x - u.x * v.z,
                     u.x * v.y - u.y * v.x};
    return {2.0f * dotUV * u.x + (q.w * q.w - dotUU) * v.x + 2.0f * q.w * cross.x,
            2.0f * dotUV * u.y + (q.w * q.w - dotUU) * v.y + 2.0f * q.w * cross.y,
            2.0f * dotUV * u.z + (q.w * q.w - dotUU) * v.z + 2.0f * q.w * cross.z};
}

float Length(Vec3 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

Vec3 Normalized(Vec3 v)
{
    const float length = Length(v);
    if (length <= 1e-8f)
        return {};
    return {v.x / length, v.y / length, v.z / length};
}

Vec3 Mul(Vec3 a, Vec3 b)
{
    return {a.x * b.x, a.y * b.y, a.z * b.z};
}

Vec3 Add(Vec3 a, Vec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 Sub(Vec3 a, Vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Scale(Vec3 v, float s)
{
    return {v.x * s, v.y * s, v.z * s};
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

struct GlobalJoint
{
    Vec3 position{};
    Quaternion rotation{0, 0, 0, 1};
    Vec3 scale{1, 1, 1};
};

std::vector<GlobalJoint> Globals(const SkeletonAsset &skeleton)
{
    std::vector<GlobalJoint> globals(skeleton.joints.size());
    for (unsigned i = 0; i < skeleton.joints.size(); ++i)
    {
        const auto &joint = skeleton.joints[i];
        const auto &local = joint.local;
        auto &global = globals[i];
        if (joint.parent < 0)
        {
            global.position = local.translation;
            global.rotation = Normalize(local.rotation);
            global.scale = local.scale;
            continue;
        }

        const auto &parent = globals[unsigned(joint.parent)];
        global.position = Add(parent.position,
                              Rotate(parent.rotation, Mul(parent.scale, local.translation)));
        global.rotation = MultiplyQ(parent.rotation, local.rotation);
        global.scale = Mul(parent.scale, local.scale);
    }
    return globals;
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

    float translationScale = config.translationScale;
    if (translationScale <= 0)
    {
        for (const auto &targetJoint : target->skeleton.joints)
            if (IsPelvis(targetJoint.name))
            {
                if (auto found = resolveSource(targetJoint.name))
                {
                    const float s = Length(source->skeleton.joints[*found].local.translation);
                    const float t = Length(targetJoint.local.translation);
                    if (s > 1e-5f && t > 1e-5f)
                        translationScale = t / s;
                }
                break;
            }
    }
    if (translationScale <= 0 || !std::isfinite(translationScale))
        translationScale = 1.0f;

    auto result = std::make_shared<ModelAsset>(*target.Shared());
    result->animations.clear();
    result->animations.reserve(source->animations.size());

    unsigned mappedCount = 0;
    for (const auto &targetJoint : target->skeleton.joints)
        if (resolveSource(targetJoint.name))
            ++mappedCount;

    // An explicit humanoid alias map means the rigs have different local bases.
    // In that case neutral-reference rebasing is the safe default: otherwise the
    // source motion delta is composed on the target's authored gameplay/bind pose.
    const bool rebaseReferencePose = config.adoptSourceReferencePose || !config.jointMap.empty();

    if (rebaseReferencePose)
    {
        const auto sourceGlobals = Globals(source->skeleton);
        const auto originalTargetGlobals = Globals(target->skeleton);

        std::optional<unsigned> targetAnchor;
        std::optional<unsigned> sourceAnchor;
        for (unsigned i = 0; i < target->skeleton.joints.size(); ++i)
            if (IsPelvis(target->skeleton.joints[i].name))
                if (auto mapped = resolveSource(target->skeleton.joints[i].name))
                {
                    targetAnchor = i;
                    sourceAnchor = *mapped;
                    break;
                }

        if (!targetAnchor || !sourceAnchor)
            throw std::invalid_argument("neutral retarget pose requires a mapped pelvis");

        const Quaternion globalAlignment = MultiplyQ(
            originalTargetGlobals[*targetAnchor].rotation,
            Conjugate(sourceGlobals[*sourceAnchor].rotation));

        std::vector<GlobalJoint> rebasedGlobals(result->skeleton.joints.size());
        for (unsigned i = 0; i < result->skeleton.joints.size(); ++i)
        {
            auto &joint = result->skeleton.joints[i];
            const auto &original = target->skeleton.joints[i].local;
            auto mapped = resolveSource(joint.name);

            if (mapped && !IsRoot(joint.name))
            {
                const unsigned sourceIndex = *mapped;
                const Quaternion desiredGlobalRotation =
                    MultiplyQ(globalAlignment, sourceGlobals[sourceIndex].rotation);

                if (joint.parent >= 0)
                {
                    const unsigned parentIndex = unsigned(joint.parent);
                    const auto &parentGlobal = rebasedGlobals[parentIndex];
                    joint.local.rotation = MultiplyQ(Conjugate(parentGlobal.rotation),
                                                     desiredGlobalRotation);

                    unsigned sourceParent = sourceIndex;
                    if (auto mappedParent = resolveSource(result->skeleton.joints[parentIndex].name))
                        sourceParent = *mappedParent;
                    else if (source->skeleton.joints[sourceIndex].parent >= 0)
                        sourceParent = unsigned(source->skeleton.joints[sourceIndex].parent);

                    if (!IsPelvis(joint.name) && sourceParent != sourceIndex)
                    {
                        Vec3 direction = Sub(sourceGlobals[sourceIndex].position,
                                             sourceGlobals[sourceParent].position);
                        direction = Rotate(globalAlignment, Normalized(direction));
                        direction = Rotate(Conjugate(parentGlobal.rotation), direction);
                        const float targetLength = Length(original.translation);
                        if (Length(direction) > 1e-6f && targetLength > 1e-6f)
                            joint.local.translation = Scale(Normalized(direction), targetLength);
                    }
                }
            }

            const auto &local = joint.local;
            auto &global = rebasedGlobals[i];
            if (joint.parent < 0)
            {
                global.position = local.translation;
                global.rotation = Normalize(local.rotation);
                global.scale = local.scale;
            }
            else
            {
                const auto &parent = rebasedGlobals[unsigned(joint.parent)];
                global.position = Add(parent.position,
                                      Rotate(parent.rotation, Mul(parent.scale, local.translation)));
                global.rotation = MultiplyQ(parent.rotation, local.rotation);
                global.scale = Mul(parent.scale, local.scale);
            }
        }
    }

    for (const auto &sourceClip : source->animations)
    {
        AnimationClip clip;
        clip.name = sourceClip.name;
        clip.duration = sourceClip.duration;
        clip.tracks.resize(result->skeleton.joints.size());

        for (unsigned targetIndex = 0; targetIndex < result->skeleton.joints.size(); ++targetIndex)
        {
            const auto &targetJoint = result->skeleton.joints[targetIndex];
            auto &destination = clip.tracks[targetIndex];
            auto sourceIndex = resolveSource(targetJoint.name);
            if (!sourceIndex)
            {
                destination.translations.push_back({0, targetJoint.local.translation});
                destination.rotations.push_back({0, targetJoint.local.rotation});
                destination.scales.push_back({0, targetJoint.local.scale});
                continue;
            }

            const auto &sourceJointNode = source->skeleton.joints[*sourceIndex];
            const auto &sourceTrack = sourceClip.tracks[*sourceIndex];
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
                const Quaternion retargeted = MultiplyQ(
                    targetJoint.local.rotation,
                    MultiplyQ(sourceBindInverse, Normalize(key.value)));
                destination.rotations.push_back({key.time, retargeted});
            }

            destination.scales.push_back({0, targetJoint.local.scale});
        }

        result->animations.push_back(std::move(clip));
    }

    result->metadata.path = target->metadata.path + "#retarget:" + source->metadata.path;
    result->metadata.generation = std::max(target->metadata.generation, source->metadata.generation);
    result->warnings.push_back("Animations retargeted at runtime from " + source->metadata.path +
                               " using " + std::to_string(mappedCount) + " mapped joints" +
                               (rebaseReferencePose ? " with neutral reference rebase" : ""));
    Validate(*result);
    return AssetHandle<ModelAsset>(std::move(result));
}
} // namespace PlagC
