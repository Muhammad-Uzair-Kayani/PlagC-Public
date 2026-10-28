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

Quaternion Slerp(Quaternion a, Quaternion b, float t)
{
    a = Normalize(a);
    b = Normalize(b);
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    if (dot < 0.0f)
    {
        b = {-b.x, -b.y, -b.z, -b.w};
        dot = -dot;
    }

    dot = std::clamp(dot, -1.0f, 1.0f);
    if (dot > 0.9995f)
        return Normalize({a.x + (b.x - a.x) * t,
                          a.y + (b.y - a.y) * t,
                          a.z + (b.z - a.z) * t,
                          a.w + (b.w - a.w) * t});

    const float theta = std::acos(dot);
    const float sinTheta = std::sin(theta);
    if (std::abs(sinTheta) <= 1e-8f)
        return a;
    const float wa = std::sin((1.0f - t) * theta) / sinTheta;
    const float wb = std::sin(t * theta) / sinTheta;
    return Normalize({a.x * wa + b.x * wb,
                      a.y * wa + b.y * wb,
                      a.z * wa + b.z * wb,
                      a.w * wa + b.w * wb});
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

Quaternion SampleRotation(const JointTrack &track, Quaternion fallback, float time)
{
    if (track.rotations.empty())
        return Normalize(fallback);
    if (track.rotations.size() == 1 || time <= track.rotations.front().time)
        return Normalize(track.rotations.front().value);
    if (time >= track.rotations.back().time)
        return Normalize(track.rotations.back().value);

    auto upper = std::upper_bound(track.rotations.begin(), track.rotations.end(), time,
                                  [](float value, const RotationKey &key) {
                                      return value < key.time;
                                  });
    const auto &b = *upper;
    const auto &a = *(upper - 1);
    const float span = b.time - a.time;
    const float alpha = span > 1e-8f ? (time - a.time) / span : 0.0f;
    return Slerp(a.value, b.value, alpha);
}

Vec3 SampleTranslation(const JointTrack &track, Vec3 fallback, float time)
{
    if (track.translations.empty())
        return fallback;
    if (track.translations.size() == 1 || time <= track.translations.front().time)
        return track.translations.front().value;
    if (time >= track.translations.back().time)
        return track.translations.back().value;

    auto upper = std::upper_bound(track.translations.begin(), track.translations.end(), time,
                                  [](float value, const VectorKey &key) {
                                      return value < key.time;
                                  });
    const auto &b = *upper;
    const auto &a = *(upper - 1);
    const float span = b.time - a.time;
    const float alpha = span > 1e-8f ? (time - a.time) / span : 0.0f;
    return {a.value.x + (b.value.x - a.value.x) * alpha,
            a.value.y + (b.value.y - a.value.y) * alpha,
            a.value.z + (b.value.z - a.value.z) * alpha};
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

std::vector<Quaternion> ReferenceGlobalRotations(const ModelAsset &model,
                                                 const AnimationClip *reference)
{
    std::vector<Quaternion> globals(model.skeleton.joints.size());
    for (unsigned i = 0; i < model.skeleton.joints.size(); ++i)
    {
        const auto local = ReferenceTransform(model, reference, i).rotation;
        const int parent = model.skeleton.joints[i].parent;
        globals[i] = parent < 0 ? Normalize(local)
                                : MultiplyQ(globals[unsigned(parent)], local);
    }
    return globals;
}

std::vector<Quaternion> AnimatedGlobalRotations(const ModelAsset &model,
                                                const AnimationClip &clip,
                                                float time)
{
    std::vector<Quaternion> globals(model.skeleton.joints.size());
    for (unsigned i = 0; i < model.skeleton.joints.size(); ++i)
    {
        Quaternion local = model.skeleton.joints[i].local.rotation;
        if (i < clip.tracks.size())
            local = SampleRotation(clip.tracks[i], local, time);
        const int parent = model.skeleton.joints[i].parent;
        globals[i] = parent < 0 ? Normalize(local)
                                : MultiplyQ(globals[unsigned(parent)], local);
    }
    return globals;
}

std::vector<float> RotationTimes(const AnimationClip &clip,
                                 const std::vector<std::optional<unsigned>> &mapping)
{
    std::vector<float> times{0.0f};
    for (const auto &mapped : mapping)
    {
        if (!mapped || *mapped >= clip.tracks.size())
            continue;
        for (const auto &key : clip.tracks[*mapped].rotations)
            times.push_back(key.time);
    }

    std::sort(times.begin(), times.end());
    times.erase(std::unique(times.begin(), times.end(), [](float a, float b) {
                    return std::abs(a - b) <= 1e-5f;
                }),
                times.end());
    return times;
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

    std::vector<std::optional<unsigned>> mapping(target->skeleton.joints.size());
    unsigned mappedCount = 0;
    for (unsigned i = 0; i < target->skeleton.joints.size(); ++i)
    {
        mapping[i] = resolveSource(target->skeleton.joints[i].name);
        if (mapping[i])
            ++mappedCount;
    }

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
        if (auto *neutral = FindClip(*source, "A_TPose"))
        {
            sourceReference = neutral;
            sourceReferenceName = "A_TPose";
        }
    }

    float translationScale = config.translationScale;
    if (translationScale <= 0)
    {
        for (unsigned i = 0; i < target->skeleton.joints.size(); ++i)
            if (IsPelvis(target->skeleton.joints[i].name) && mapping[i])
            {
                const JointTransform sourceRef =
                    ReferenceTransform(*source, sourceReference, *mapping[i]);
                const float s = Length(sourceRef.translation);
                const float t = Length(target->skeleton.joints[i].local.translation);
                if (s > 1e-5f && t > 1e-5f)
                    translationScale = t / s;
                break;
            }
    }
    if (translationScale <= 0 || !std::isfinite(translationScale))
        translationScale = 1.0f;

    // The target skeleton's unanimated transforms are its authored A/T reference.
    // Do not rewrite them. The source uses its explicit A_TPose reference clip.
    const auto targetReferenceGlobals = ReferenceGlobalRotations(*target, nullptr);
    const auto sourceReferenceGlobals = ReferenceGlobalRotations(*source, sourceReference);

    auto result = std::make_shared<ModelAsset>(*target.Shared());
    if (!config.preserveTargetAnimations)
        result->animations.clear();
    result->animations.reserve(result->animations.size() + source->animations.size());

    for (const auto &sourceClip : source->animations)
    {
        AnimationClip clip;
        clip.name = sourceClip.name;
        clip.duration = sourceClip.duration;
        clip.tracks.resize(target->skeleton.joints.size());

        // Every mapped joint is sampled on one shared timeline. This is important:
        // a child's target-local rotation depends on its animated parent's global
        // rotation at the same instant.
        const auto times = RotationTimes(sourceClip, mapping);
        for (float time : times)
        {
            const auto sourceAnimatedGlobals = AnimatedGlobalRotations(*source, sourceClip, time);
            std::vector<Quaternion> targetAnimatedGlobals(target->skeleton.joints.size());

            for (unsigned i = 0; i < target->skeleton.joints.size(); ++i)
            {
                const int parent = target->skeleton.joints[i].parent;
                if (mapping[i])
                {
                    const unsigned sourceIndex = *mapping[i];
                    // Global reference alignment:
                    // targetAnimated = targetReference * inverse(sourceReference) * sourceAnimated.
                    // This transfers the source absolute joint orientation through the
                    // two neutral reference poses, so differing local bone axes do not
                    // get multiplied directly into each other.
                    targetAnimatedGlobals[i] = MultiplyQ(
                        MultiplyQ(targetReferenceGlobals[i],
                                  Conjugate(sourceReferenceGlobals[sourceIndex])),
                        sourceAnimatedGlobals[sourceIndex]);
                }
                else if (parent < 0)
                {
                    targetAnimatedGlobals[i] =
                        Normalize(target->skeleton.joints[i].local.rotation);
                }
                else
                {
                    targetAnimatedGlobals[i] = MultiplyQ(
                        targetAnimatedGlobals[unsigned(parent)],
                        target->skeleton.joints[i].local.rotation);
                }
            }

            for (unsigned i = 0; i < target->skeleton.joints.size(); ++i)
            {
                const int parent = target->skeleton.joints[i].parent;
                Quaternion local = parent < 0
                                       ? targetAnimatedGlobals[i]
                                       : MultiplyQ(
                                             Conjugate(targetAnimatedGlobals[unsigned(parent)]),
                                             targetAnimatedGlobals[i]);
                clip.tracks[i].rotations.push_back({time, Normalize(local)});
            }
        }

        for (unsigned i = 0; i < target->skeleton.joints.size(); ++i)
        {
            const auto &targetJoint = target->skeleton.joints[i];
            auto &destination = clip.tracks[i];

            const bool translate = mapping[i] &&
                                   ((config.rootTranslation && IsRoot(targetJoint.name)) ||
                                    (config.pelvisTranslation && IsPelvis(targetJoint.name)));
            if (translate && *mapping[i] < sourceClip.tracks.size())
            {
                const unsigned sourceIndex = *mapping[i];
                const auto &sourceTrack = sourceClip.tracks[sourceIndex];
                const auto sourceRef =
                    ReferenceTransform(*source, sourceReference, sourceIndex);
                if (!sourceTrack.translations.empty())
                {
                    for (const auto &key : sourceTrack.translations)
                    {
                        const Vec3 delta{
                            (key.value.x - sourceRef.translation.x) * translationScale,
                            (key.value.y - sourceRef.translation.y) * translationScale,
                            (key.value.z - sourceRef.translation.z) * translationScale};
                        destination.translations.push_back(
                            {key.time,
                             {targetJoint.local.translation.x + delta.x,
                              targetJoint.local.translation.y + delta.y,
                              targetJoint.local.translation.z + delta.z}});
                    }
                }
            }

            if (destination.translations.empty())
                destination.translations.push_back({0, targetJoint.local.translation});
            destination.scales.push_back({0, targetJoint.local.scale});
        }

        result->animations.push_back(std::move(clip));
    }

    result->metadata.path = target->metadata.path + "#retarget:" + source->metadata.path;
    result->metadata.generation = std::max(target->metadata.generation, source->metadata.generation);
    result->warnings.push_back(
        "Animations retargeted in global reference space from " + source->metadata.path +
        " using " + std::to_string(mappedCount) + " mapped joints" +
        (sourceReferenceName.empty() ? " with source bind reference"
                                     : " using source reference clip " + sourceReferenceName) +
        "; target authored A/T reference pose preserved" +
        (config.preserveTargetAnimations ? "; native target clips preserved" : ""));
    Validate(*result);
    return AssetHandle<ModelAsset>(std::move(result));
}
} // namespace PlagC
