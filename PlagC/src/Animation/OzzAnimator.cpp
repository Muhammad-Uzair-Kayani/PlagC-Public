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
    std::vector<ozz::math::Float4x4> matrices;
    std::vector<Matrix4> pose;
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
    if (component.fadeDuration == 0)
        if (auto transition = controller.Evaluate())
        {
            component.previous = component.state;
            const auto &node = controller.Current();
            component.state = {node.clip, 0, node.speed, true, node.loop};
            component.fadeElapsed = 0;
            component.fadeDuration = transition->crossfade;
        }
    if (component.fadeDuration <= 0)
    {
        Update(component.state, dt);
        return;
    }
    auto &p = *impl_;
    Update(component.previous, dt);
    p.blendSource = p.locals;
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
}
std::span<const Matrix4> Animator::Pose() const
{
    return impl_->pose;
}
MeshData Animator::Deform(unsigned mesh) const
{
    auto &p = *impl_;
    const auto &src = p.model->meshes.at(mesh);
    MeshData output = src.geometry;
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
        auto transform = p.pose.at(src.node);
        for (auto &v : output.vertices)
        {
            v.position = TransformPoint(transform, v.position);
            v.normal = TransformNormal(transform, v.normal);
            v.tangent = tangent(direction(transform, v.tangent), v.normal);
        }
    }
    else
    {
        std::vector<Matrix4> skin(p.pose.size());
        for (unsigned i = 0; i < skin.size(); ++i)
            skin[i] = Multiply(p.pose[i], src.inverseBind[i]);
        for (unsigned i = 0; i < output.vertices.size(); ++i)
        {
            auto original = output.vertices[i];
            Vec3 pos{}, normal{}, tan{};
            for (unsigned k = 0; k < 4; ++k)
            {
                float weight = src.skin[i].weights[k];
                if (weight == 0)
                    continue;
                auto &m = skin[src.skin[i].joints[k]];
                auto v = TransformPoint(m, original.position),
                     n = TransformNormal(m, original.normal);
                pos.x += weight * v.x;
                pos.y += weight * v.y;
                pos.z += weight * v.z;
                normal.x += weight * n.x;
                normal.y += weight * n.y;
                normal.z += weight * n.z;
                auto t = direction(m, original.tangent);
                tan.x += weight * t.x;
                tan.y += weight * t.y;
                tan.z += weight * t.z;
            }
            float length =
                std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
            if (length > 1e-8f)
            {
                normal.x /= length;
                normal.y /= length;
                normal.z /= length;
            }
            output.vertices[i].position = pos;
            output.vertices[i].normal = normal;
            output.vertices[i].tangent = tangent(tan, normal);
        }
    }
    return output;
}
} // namespace PlagC
