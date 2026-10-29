#include "pgcpch.h"
#include <PlagC/Animation/Animation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/maths/simd_math.h>
namespace PlagC
{
struct Animator::Impl
{
    AssetHandle<ModelAsset> model;
    ozz::unique_ptr<ozz::animation::Skeleton> skeleton;
    std::vector<ozz::unique_ptr<ozz::animation::Animation>> clips;
    ozz::animation::SamplingJob::Context context;
    std::vector<ozz::math::SoaTransform> locals, blendSource, blendTarget;
    std::vector<ozz::math::SoaTransform> lastBase, interruptedSource;
    bool interrupted = false;
    std::vector<ozz::math::SoaTransform> lastUpper, upperSource;
    unsigned upperClip = 0;
    double upperFade = .12;
    std::vector<ozz::math::Float4x4> matrices;
    std::vector<Matrix4> pose;
    // CPU skinning hot-path scratch. These buffers are sized once per Animator and reused for
    // every frame so DeformVertices performs no heap allocation and does not copy mesh indices.
    std::vector<Matrix4> skinMatrices;
    std::vector<std::vector<Vertex>> deformedVertices;
    explicit Impl(AssetHandle<ModelAsset> a) : model(std::move(a))
    {
        Validate(*model);
        const auto &joints = model->skeleton.joints;
        ozz::animation::offline::RawSkeleton raw;
        std::function<ozz::animation::offline::RawSkeleton::Joint(unsigned)> build =
            [&](unsigned i) {
                ozz::animation::offline::RawSkeleton::Joint j;
                auto &source = joints[i];
                j.name = source.name.c_str();
                auto &t = source.local;
                j.transform.translation = {t.translation.x, t.translation.y, t.translation.z};
                j.transform.rotation = {t.rotation.x, t.rotation.y, t.rotation.z, t.rotation.w};
                j.transform.scale = {t.scale.x, t.scale.y, t.scale.z};
                for (unsigned k = i + 1; k < joints.size(); ++k)
                    if (joints[k].parent == int(i))
                        j.children.push_back(build(k));
                return j;
            };
        for (unsigned i = 0; i < joints.size(); ++i)
            if (joints[i].parent < 0)
                raw.roots.push_back(build(i));
        skeleton = ozz::animation::offline::SkeletonBuilder{}(raw);
        if (!skeleton)
            throw std::runtime_error("Ozz skeleton build failed");
        for (unsigned i = 0; i < joints.size(); ++i)
            if (skeleton->joint_parents()[i] != joints[i].parent ||
                joints[i].name != skeleton->joint_names()[i])
                throw std::runtime_error("skeleton nodes must be depth-first ordered");
        for (auto &src : model->animations)
        {
            ozz::animation::offline::RawAnimation rawClip;
            rawClip.name = src.name.c_str();
            rawClip.duration = src.duration;
            rawClip.tracks.resize(src.tracks.size());
            for (unsigned i = 0; i < src.tracks.size(); ++i)
            {
                auto &dest = rawClip.tracks[i];
                auto &track = src.tracks[i];
                for (auto &k : track.translations)
                    dest.translations.push_back({k.time, {k.value.x, k.value.y, k.value.z}});
                for (auto &k : track.rotations)
                    dest.rotations.push_back(
                        {k.time, {k.value.x, k.value.y, k.value.z, k.value.w}});
                for (auto &k : track.scales)
                    dest.scales.push_back({k.time, {k.value.x, k.value.y, k.value.z}});
            }
            auto clip = ozz::animation::offline::AnimationBuilder{}(rawClip);
            if (!clip)
                throw std::runtime_error("Ozz animation build failed");
            clips.push_back(std::move(clip));
        }
        locals.resize(skeleton->num_soa_joints());
        blendSource.resize(locals.size());
        blendTarget.resize(locals.size());
        matrices.resize(joints.size());
        pose.resize(joints.size());
        skinMatrices.resize(joints.size());
        deformedVertices.resize(model->meshes.size());
        for (unsigned i = 0; i < model->meshes.size(); ++i)
            deformedVertices[i].resize(model->meshes[i].geometry.vertices.size());
        context.Resize(skeleton->num_joints());
        std::copy(skeleton->joint_rest_poses().begin(), skeleton->joint_rest_poses().end(),
                  locals.begin());
        ModelPose();
    }
    void ModelPose()
    {
        ozz::animation::LocalToModelJob job;
        job.skeleton = skeleton.get();
        job.input = ozz::make_span(locals);
        job.output = ozz::make_span(matrices);
        if (!job.Run())
            throw std::runtime_error("Ozz local-to-model failed");
        for (unsigned i = 0; i < pose.size(); ++i)
            for (unsigned c = 0; c < 4; ++c)
                ozz::math::StorePtrU(matrices[i].cols[c], pose[i].m.data() + c * 4);
    }
};
Animator::Animator(AssetHandle<ModelAsset> model) : impl_(std::make_unique<Impl>(std::move(model)))
{
}
Animator::~Animator() = default;
void Animator::Update(AnimationState &state, double dt)
{
    auto &p = *impl_;
    if (!std::isfinite(dt) || dt < 0 || !std::isfinite(state.time) || !std::isfinite(state.speed))
        throw std::invalid_argument("invalid animation clock");
    if (p.clips.empty())
        return;
    if (state.clip >= p.clips.size())
        throw std::invalid_argument("animation clip out of range");
    auto &clip = p.clips[state.clip];
    if (state.playing)
        state.time += dt * state.speed;
    double duration = clip->duration();
    if (state.loop)
    {
        state.time = std::fmod(state.time, duration);
        if (state.time < 0)
            state.time += duration;
    }
    else
    {
        state.time = std::clamp(state.time, 0., duration);
        if (state.time == duration && state.speed > 0)
            state.playing = false;
    }
    ozz::animation::SamplingJob sample;
    sample.animation = clip.get();
    sample.context = &p.context;
    sample.ratio = float(state.time / duration);
    sample.output = ozz::make_span(p.locals);
    if (!sample.Run())
        throw std::runtime_error("Ozz sampling failed");
    p.ModelPose();
}
void Animator::Update(AnimatorComponent &component, double dt)
{
    if (!std::isfinite(dt) || dt < 0)
        throw std::invalid_argument("invalid animation timestep");
    if (!component.controller)
    {
        Update(component.state, dt);
        ApplyUpperBody(component, dt);
        return;
    }
    auto &controller = *component.controller;
    if (!component.controllerStarted)
    {
        controller.Graph().Validate(unsigned(impl_->clips.size()));
        const auto &node = controller.Current();
        component.state = {node.clip, 0, node.speed, true, node.loop};
        component.controllerStarted = true;
    }
    if (auto transition = controller.Evaluate())
    {
        impl_->interrupted = component.fadeDuration > 0 && !impl_->lastBase.empty();
        if (impl_->interrupted)
            impl_->interruptedSource = impl_->lastBase;
        component.previous = component.state;
        const auto &node = controller.Current();
        component.state = {node.clip, 0, node.speed, true, node.loop};
        component.fadeElapsed = 0;
        component.fadeDuration = transition->crossfade;
    }
    if (component.fadeDuration <= 0)
    {
        Update(component.state, dt);
        ApplyUpperBody(component, dt);
        return;
    }
    auto &p = *impl_;
    if (p.interrupted)
        p.blendSource = p.interruptedSource;
    else
    {
        Update(component.previous, dt);
        p.blendSource = p.locals;
    }
    Update(component.state, dt);
    p.blendTarget = p.locals;
    component.fadeElapsed = std::min(component.fadeDuration, component.fadeElapsed + dt);
    float weight = float(component.fadeElapsed / component.fadeDuration);
    ozz::animation::BlendingJob::Layer layers[2];
    layers[0].weight = 1 - weight;
    layers[0].transform = ozz::make_span(p.blendSource);
    layers[1].weight = weight;
    layers[1].transform = ozz::make_span(p.blendTarget);
    ozz::animation::BlendingJob blend;
    blend.layers = ozz::make_span(layers);
    blend.rest_pose = p.skeleton->joint_rest_poses();
    blend.output = ozz::make_span(p.locals);
    if (!blend.Run())
        throw std::runtime_error("Ozz blend failed");
    p.ModelPose();
    if (weight >= 1)
        component.fadeDuration = 0;
    ApplyUpperBody(component, dt);
}
void Animator::ApplyUpperBody(AnimatorComponent &component, double dt)
{
    impl_->lastBase = impl_->locals;
    if (!component.upperBody)
    {
        impl_->lastUpper.clear();
        return;
    }
    auto &layer = *component.upperBody;
    if (!std::isfinite(layer.weight) || layer.weight < 0 || layer.weight > 1 ||
        !std::isfinite(layer.recoilRadians))
        throw std::invalid_argument("invalid upper body layer");
    auto &p = *impl_;
    const auto &joints = p.model->skeleton.joints;
    auto root = std::find_if(joints.begin(), joints.end(),
                             [&](const auto &j) { return j.name == layer.root; });
    if (root == joints.end())
        throw std::invalid_argument("upper body root missing");
    if (layer.weight == 0)
    {
        p.lastUpper.clear();
        return;
    }
    const unsigned rootIndex = unsigned(root - joints.begin());
    p.blendSource = p.locals;
    // The caller supplies the weapon clock (held aim or normalized reload).
    Update(layer.pose, 0);
    p.blendTarget = p.locals;
    std::vector<ozz::math::SimdFloat4> mask(p.locals.size());
    for (unsigned block = 0; block < mask.size(); ++block)
    {
        float weights[4]{};
        float rx[4]{}, rw[4]{1, 1, 1, 1};
        for (unsigned lane = 0; lane < 4; ++lane)
        {
            unsigned i = block * 4 + lane;
            if (i >= joints.size())
                continue;
            for (int ancestor = int(i); ancestor >= 0; ancestor = joints[ancestor].parent)
                if (unsigned(ancestor) == rootIndex)
                {
                    weights[lane] = 1;
                    break;
                }
            if (i == rootIndex)
            {
                rx[lane] = std::sin(layer.recoilRadians * .5f);
                rw[lane] = std::cos(layer.recoilRadians * .5f);
            }
        }
        mask[block] = ozz::math::simd_float4::LoadPtrU(weights);
        auto recoil = ozz::math::SoaQuaternion::Load(
            ozz::math::simd_float4::LoadPtrU(rx), ozz::math::simd_float4::zero(),
            ozz::math::simd_float4::zero(), ozz::math::simd_float4::LoadPtrU(rw));
        p.blendTarget[block].rotation = p.blendTarget[block].rotation * recoil;
    }
    // Snapshot the displayed weapon pose when changing actions, including when
    // interrupting an existing fade. Never blend reload through an Idle state.
    if (!p.lastUpper.empty() && p.upperClip != layer.pose.clip)
    {
        p.upperSource = p.lastUpper;
        p.upperFade = 0;
    }
    if (p.lastUpper.empty())
        p.upperFade = .12;
    p.upperClip = layer.pose.clip;
    if (p.upperFade < .12)
    {
        p.upperFade = std::min(.12, p.upperFade + dt);
        const float alpha = float(p.upperFade / .12);
        ozz::animation::BlendingJob::Layer actionLayers[2];
        actionLayers[0].weight = 1 - alpha;
        actionLayers[0].transform = ozz::make_span(p.upperSource);
        actionLayers[1].weight = alpha;
        actionLayers[1].transform = ozz::make_span(p.blendTarget);
        ozz::animation::BlendingJob actionBlend;
        actionBlend.layers = ozz::make_span(actionLayers);
        actionBlend.rest_pose = p.skeleton->joint_rest_poses();
        actionBlend.output = ozz::make_span(p.locals);
        if (!actionBlend.Run())
            throw std::runtime_error("weapon action blend failed");
        p.blendTarget = p.locals;
    }
    p.lastUpper = p.blendTarget;
    ozz::animation::BlendingJob::Layer layers[2];
    layers[0].weight = 1;
    layers[0].transform = ozz::make_span(p.blendSource);
    // Complementary per-joint weights preserve the base exactly below the mask.
    std::vector<ozz::math::SimdFloat4> baseMask(mask.size());
    for (unsigned i = 0; i < mask.size(); ++i)
    {
        mask[i] = mask[i] * ozz::math::simd_float4::Load1(layer.weight);
        baseMask[i] = ozz::math::simd_float4::one() - mask[i];
    }
    layers[0].joint_weights = ozz::make_span(baseMask);
    layers[1].weight = 1;
    layers[1].transform = ozz::make_span(p.blendTarget);
    layers[1].joint_weights = ozz::make_span(mask);
    ozz::animation::BlendingJob blend;
    blend.layers = ozz::make_span(layers);
    blend.rest_pose = p.skeleton->joint_rest_poses();
    blend.output = ozz::make_span(p.locals);
    if (!blend.Run())
        throw std::runtime_error("upper body blend failed");
    p.ModelPose();
}
std::span<const Matrix4> Animator::Pose() const
{
    return impl_->pose;
}
std::span<const Vertex> Animator::DeformVertices(unsigned mesh) const
{
    auto &p = *impl_;
    const auto &src = p.model->meshes.at(mesh);
    auto &output = p.deformedVertices.at(mesh);
    if (output.size() != src.geometry.vertices.size())
        output.resize(src.geometry.vertices.size());

    auto direction = [](const Matrix4 &m, Vec3 v) {
        return Vec3{m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z,
                    m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z,
                    m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z};
    };
    auto tangent = [](Vec3 t, Vec3 n) {
        float d = t.x * n.x + t.y * n.y + t.z * n.z;
        t = {t.x - d * n.x, t.y - d * n.y, t.z - d * n.z};
        float l = std::sqrt(t.x * t.x + t.y * t.y + t.z * t.z);
        return l > 1e-8f ? Vec3{t.x / l, t.y / l, t.z / l} : Vec3{1, 0, 0};
    };
    if (src.skin.empty())
    {
        const auto transform = p.pose.at(src.node);
        for (unsigned i = 0; i < output.size(); ++i)
        {
            const auto &original = src.geometry.vertices[i];
            auto &v = output[i];
            v = original;
            v.position = TransformPoint(transform, original.position);
            v.normal = TransformNormal(transform, original.normal);
            v.tangent = tangent(direction(transform, original.tangent), v.normal);
        }
    }
    else
    {
        if (p.skinMatrices.size() != p.pose.size())
            p.skinMatrices.resize(p.pose.size());
        for (unsigned i = 0; i < p.skinMatrices.size(); ++i)
            p.skinMatrices[i] = Multiply(p.pose[i], src.inverseBind[i]);

        for (unsigned i = 0; i < output.size(); ++i)
        {
            const auto &original = src.geometry.vertices[i];
            auto &result = output[i];
            result = original;
            Vec3 pos{}, normal{}, tan{};
            for (unsigned k = 0; k < 4; ++k)
            {
                const float weight = src.skin[i].weights[k];
                if (weight == 0)
                    continue;
                const auto &m = p.skinMatrices[src.skin[i].joints[k]];
                const auto v = TransformPoint(m, original.position);
                const auto n = TransformNormal(m, original.normal);
                pos.x += weight * v.x;
                pos.y += weight * v.y;
                pos.z += weight * v.z;
                normal.x += weight * n.x;
                normal.y += weight * n.y;
                normal.z += weight * n.z;
                const auto t = direction(m, original.tangent);
                tan.x += weight * t.x;
                tan.y += weight * t.y;
                tan.z += weight * t.z;
            }
            const float length =
                std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            if (length > 1e-8f)
            {
                normal.x /= length;
                normal.y /= length;
                normal.z /= length;
            }
            result.position = pos;
            result.normal = normal;
            result.tangent = tangent(tan, normal);
        }
    }
    return output;
}
MeshData Animator::Deform(unsigned mesh) const
{
    const auto &src = impl_->model->meshes.at(mesh);
    MeshData output = src.geometry;
    const auto vertices = DeformVertices(mesh);
    output.vertices.assign(vertices.begin(), vertices.end());
    return output;
}
} // namespace PlagC
