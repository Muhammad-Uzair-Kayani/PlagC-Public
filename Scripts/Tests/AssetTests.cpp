#include "pgcpch.h"
#include <PlagC/Assets/AssetManager.h>
#include <PlagC/Assets/AssetScene.h>
#include <PlagC/Animation/Animation.h>
#include <PlagC/Audio/AudioSystem.h>
#include <PlagC/Thread.h>
#include <limits>
#include <PlagC/Camera/CameraSystem.h>
using namespace PlagC;
void Check(bool value, const char *name)
{
    if (!value)
        throw std::runtime_error(name);
    std::cout << "[PASS] " << name << std::endl;
}
template <class F> void Reject(F f, const char *name)
{
    bool rejected = false;
    try
    {
        f();
    }
    catch (const std::exception &)
    {
        rejected = true;
    }
    Check(rejected, name);
}
bool Close(Vec3 a, Vec3 b)
{
    return std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z) < .001f;
}
int main()
{
    Thread::InitializeMain();
    try
    {
        const std::filesystem::path root = "Bin-Int/AssetTests/Cooked";
        std::filesystem::create_directories(root);
        auto imported = ImportModel("Assets/Source/Beacon.gltf");
        Check(!imported.meshes.empty() && imported.skeleton.joints.size() == 4 &&
                  imported.animations.size() == 1,
              "Assimp glTF imports meshes, hierarchy, skin and clip into PlagC data");
        Check(imported.textures.size() == 1 && imported.textures[0].rgba.size() == 16,
              "embedded PNG decoded to engine-owned RGBA");
        SaveAsset(imported, root / "Beacon.plasset");
        auto decoded = std::dynamic_pointer_cast<ModelAsset>(ReadAsset(root / "Beacon.plasset"));
        Check(decoded && decoded->meshes[0].skin.size() == imported.meshes[0].skin.size() &&
                  decoded->textures[0].rgba == imported.textures[0].rgba &&
                  decoded->animations[0].tracks.size() == 4,
              "versioned cooked roundtrip retains skin, textures and tracks");
        auto invalid = imported;
        invalid.meshes[0].geometry.indices[0] = 999999;
        Reject([&] { SaveAsset(invalid, root / "invalid.plasset"); },
               "invalid topology rejected before writing");
        invalid = imported;
        invalid.skeleton.joints[0].parent = 0;
        Reject([&] { Validate(invalid); }, "cyclic skeleton rejected");
        invalid = imported;
        invalid.meshes[0].skin[0].weights[0] = std::numeric_limits<float>::quiet_NaN();
        Reject([&] { Validate(invalid); }, "nonfinite skin weights rejected");
        std::filesystem::copy_file(root / "Beacon.plasset", root / "Corrupt.plasset",
                                   std::filesystem::copy_options::overwrite_existing);
        {
            std::fstream f(root / "Corrupt.plasset",
                           std::ios::in | std::ios::out | std::ios::binary);
            f.seekp(32);
            char v = 127;
            f.write(&v, 1);
        }
        Reject([&] { ReadAsset(root / "Corrupt.plasset"); },
               "corrupt payload rejected by checksum");
        {
            std::ofstream f(root / "Short.plasset", std::ios::binary);
            f << "PLAGCAST";
        }
        Reject([&] { ReadAsset(root / "Short.plasset"); }, "truncated cooked input rejected");
        AudioClip audio;
        audio.sampleRate = 48000;
        audio.channels = 1;
        for (unsigned i = 0; i < 48000; ++i)
            audio.samples.push_back(
                std::int16_t(12000 * std::sin(2 * 3.141592653589793 * 440 * i / 48000)));
        SaveAsset(audio, root / "Chime.plasset");
        // PCM WAV fixture exercises the source decoder independently of cooked PCM.
        {
            std::ofstream f(root / "tone.wav", std::ios::binary);
            auto u16 = [&](unsigned n) {
                f.put(char(n));
                f.put(char(n >> 8));
            };
            auto u32 = [&](unsigned n) {
                u16(n);
                u16(n >> 16);
            };
            f.write("RIFF", 4);
            u32(36 + unsigned(audio.samples.size() * 2));
            f.write("WAVEfmt ", 8);
            u32(16);
            u16(1);
            u16(1);
            u32(48000);
            u32(96000);
            u16(2);
            u16(16);
            f.write("data", 4);
            u32(unsigned(audio.samples.size() * 2));
            for (auto s : audio.samples)
                u16(std::uint16_t(s));
        }
        Check(ImportWave(root / "tone.wav").samples == audio.samples,
              "PCM16 WAV source import roundtrip");
        JobSystem jobs(2);
        AssetHandle<ModelAsset> retained;
        {
            AssetManager assets(jobs, root);
            assets.Request("Beacon.plasset", AssetType::Model);
            assets.Request("./Beacon.plasset", AssetType::Model);
            assets.Wait();
            retained = assets.Get<ModelAsset>("Beacon.plasset");
            auto second = assets.Load<ModelAsset>("Beacon.plasset");
            Check(retained && retained.Shared() == second.Shared() && assets.Registry().size() == 1,
                  "async duplicate paths share one immutable cached asset");
            Reject([&] { assets.Load<AudioClip>("Beacon.plasset"); },
                   "typed asset mismatch rejected");
            Reject([&] { assets.Request("../outside.plasset", AssetType::Model); },
                   "cooked-root traversal rejected");
            assets.Request("missing.plasset", AssetType::Model);
            assets.Wait();
            Check(assets.Registry().back().state == AssetState::Failed,
                  "asynchronous missing file becomes a reported Failed state");
            auto id = retained->metadata.id;
            auto generation = retained->metadata.generation;
            assets.Reload("Beacon.plasset");
            assets.Wait();
            auto reloaded = assets.Get<ModelAsset>("Beacon.plasset");
            Check(reloaded->metadata.id == id && reloaded->metadata.generation != generation &&
                      reloaded.Shared() != retained.Shared(),
                  "reload publishes new generation while old handles remain valid");
            {
                std::ofstream f(root / "Beacon.plasset", std::ios::binary);
                f << "broken";
            }
            assets.Reload("Beacon.plasset");
            assets.Wait();
            Check(assets.Get<ModelAsset>("Beacon.plasset").Shared() == reloaded.Shared(),
                  "failed reload retains last valid snapshot");
            SaveAsset(imported, root / "Beacon.plasset");
            assets.Unload("Beacon.plasset");
            Check(retained->meshes.size() == imported.meshes.size(),
                  "unload does not invalidate external handles");
            auto clip = assets.Load<AudioClip>("Chime.plasset");
            AudioSystem sound({true, true, 48000});
            Check(sound.Available(), "real OpenAL Soft loopback context initialized");
            AudioSourceDesc source;
            source.position = {0, 0, -2};
            source.loop = true;
            auto one = sound.CreateSource(clip, source), two = sound.CreateSource(clip, source);
            Check(sound.Statistics().buffers == 1 && sound.Statistics().sources == 2,
                  "audio sources share cached buffer and retain clip");
            sound.SetListener({});
            sound.Play(one);
            auto samples = sound.RenderLoopback(4800);
            std::int64_t energy = 0;
            for (auto s : samples)
                energy += std::abs(int(s));
            Check(energy > 10000, "OpenAL produces nonzero mixed PCM from managed clip");
            sound.Pause(one);
            Check(sound.State(one) == AudioPlayback::Paused, "audio pause state");
            sound.Play(one);
            sound.Stop(one);
            Check(sound.State(one) == AudioPlayback::Stopped, "audio stop state");
            source.pitch = 1.5f;
            source.gain = .25f;
            sound.UpdateSource(two, source);
            source.pitch = 0;
            Reject([&] { sound.UpdateSource(two, source); }, "invalid audio parameters rejected");
            sound.DestroySource(one);
            sound.DestroySource(two);
            sound.CollectUnused();
            Check(sound.Statistics().buffers == 0 && sound.Statistics().sources == 0,
                  "audio teardown releases unused buffers");
            Reject([&] { sound.Play(one); }, "stale audio handle rejected");
        }
        Check(retained && retained->skeleton.joints.size() == 4,
              "asset snapshot survives manager destruction");
        Animator animator(retained);
        auto rest = animator.Deform(0);
        AnimationState state;
        state.time = 1;
        state.playing = false;
        animator.Update(state, 0);
        auto moved = animator.Deform(0);
        bool changed = false;
        for (unsigned i = 0; i < rest.vertices.size(); ++i)
            changed |= !Close(rest.vertices[i].position, moved.vertices[i].position);
        Check(changed, "Ozz pose evaluation drives CPU skin deformation");
        animator.Update(state, 1);
        Check(Close(moved.vertices.back().position, animator.Deform(0).vertices.back().position),
              "paused animation preserves pose");
        state.loop = false;
        state.playing = true;
        animator.Update(state, 10);
        Check(!state.playing && state.time == retained->animations[0].duration,
              "nonlooping animation clamps and stops at end");
        Renderer renderer(nullptr, {});
        World world;
        AssetScene scene(renderer);
        auto a = world.Create(), b = world.Create();
        world.SetModel(a, {retained});
        world.SetModel(b, {retained});
        scene.Update(world, 0);
        auto staticStats = renderer.Statistics();
        Check(scene.Statistics().models == 1 && staticStats.meshes == retained->meshes.size() &&
                  staticStats.objects == 2 * retained->meshes.size() && staticStats.textures == 1,
              "two ECS instances share model GPU resources");
        world.SetAnimator(b, {});
        scene.Update(world, .5);
        Check(scene.Statistics().animated == 1 &&
                  renderer.Statistics().meshes == 2 * retained->meshes.size(),
              "animated instance gets private dynamic mesh buffers");
        world.Destroy(a);
        scene.Update(world, .1);
        Check(scene.Statistics().instances == 1 && renderer.Statistics().textures == 1,
              "destroying one instance preserves shared assets");
        world.Clear();
        scene.Update(world, 0);
        auto empty = renderer.Statistics();
        Check(!empty.objects && !empty.meshes && !empty.materials && !empty.textures,
              "ECS teardown releases instances before meshes, materials and textures");
        AnimationStateMachine graph;
        graph.parameters = {{"Speed", AnimationParameterType::Float, 0},
                            {"Grounded", AnimationParameterType::Bool, 0},
                            {"Wave", AnimationParameterType::Trigger, 0}};
        graph.states = {{"Idle", 0}, {"Walk", 0}, {"Wave", 0, false}};
        using Compare = AnimationCompare;
        graph.transitions = {
            {0, 1, .2f, {{"Speed", Compare::Greater, 1}, {"Grounded", Compare::Equal, 1}}},
            {1, 2, .2f, {{"Wave", Compare::Equal, 1}}},
            {2, 0, 0, {}}};
        AnimationController controller(graph, 1), independent(graph, 1);
        controller.SetFloat("Speed", 2);
        Check(!controller.Evaluate(), "bool parameter gates locomotion transition");
        controller.SetBool("Grounded", true);
        Check(controller.Evaluate().has_value() && controller.Current().name == "Walk",
              "float and bool conditions select state");
        Reject([&] { controller.SetBool("Speed", true); },
               "typed animation parameters reject mismatches");
        controller.Trigger("Wave");
        Check(controller.Evaluate().has_value() && controller.Current().name == "Wave",
              "trigger selects one-shot animation");
        controller.Evaluate();
        controller.Evaluate();
        Check(!controller.Evaluate(), "selected transition consumes trigger");
        Check(independent.Current().name == "Idle", "controllers retain independent state");
        AnimatorComponent blended;
        blended.controller.emplace(graph, 1);
        blended.controller->SetFloat("Speed", 2);
        blended.controller->SetBool("Grounded", true);
        animator.Update(blended, .1);
        Check(blended.fadeDuration > 0 && std::abs(blended.fadeElapsed - .1) < .001,
              "crossfade advances two-clip clock");
        animator.Update(blended, .1);
        Check(blended.fadeDuration == 0, "crossfade completes at requested duration");
        auto poseModel = std::make_shared<ModelAsset>(imported);
        poseModel->animations.push_back(poseModel->animations[0]);
        for (unsigned clip = 0; clip < 2; ++clip)
        {
            auto &track = poseModel->animations[clip].tracks[0];
            track.translations = {{0, {float(clip) * 2, 0, 0}}};
        }
        AnimationStateMachine poseGraph;
        poseGraph.parameters = {{"Go", AnimationParameterType::Trigger, 0}};
        poseGraph.states = {{"Start", 0}, {"End", 1}};
        poseGraph.transitions = {{0, 1, 1, {{"Go", Compare::Equal, 1}}}};
        Animator poseAnimator{AssetHandle<ModelAsset>(poseModel)};
        AnimatorComponent poseComponent;
        poseComponent.controller.emplace(poseGraph, 2);
        poseAnimator.Update(poseComponent, 0);
        poseComponent.controller->Trigger("Go");
        poseAnimator.Update(poseComponent, .5);
        Check(std::abs(poseAnimator.Pose()[0].m[12] - 1) < .01,
              "crossfade blends distinct joint poses at midpoint");
        poseAnimator.Update(poseComponent, .5);
        Check(std::abs(poseAnimator.Pose()[0].m[12] - 2) < .01,
              "crossfade reaches destination joint pose");
        auto camera = world.Create();
        CameraComponent initialCamera;
        initialCamera.target = {0, 0, -1};
        world.SetCamera(camera, initialCamera);
        CameraSystem cameras;
        cameras.Activate(world, camera);
        cameras.SetViewport(1600, 900);
        Check(std::abs(cameras.Extract(world).aspect - 1600.f / 900) < .001,
              "camera tracks viewport aspect");
        auto cameraComponent = *world.Read(camera).camera;
        cameraComponent.aspect = 1.25f;
        world.SetCamera(camera, cameraComponent);
        Check(cameras.Extract(world).aspect == 1.25f, "camera aspect override takes precedence");
        FPSCameraController fps;
        fps.Update(world, camera, {2, 3, 4});
        Check(Close(cameras.Extract(world).position, {2, 3, 4}),
              "FPS controller follows supplied physics eye");
        FreeCameraController free;
        free.Move(world, camera, {0, 0, 1}, .5);
        Check(Close(cameras.Extract(world).position, {2, 3, 0}), "free camera moves independently");
        world.Destroy(camera);
        Check(!world.ActiveCamera(), "destroying active camera clears selection");
        auto material = ImportModel("Assets/Source/MaterialLab.gltf");
        auto &mat = material.materials[0];
        Check(mat.baseColourTexture >= 0 && mat.normalTexture >= 0 && mat.metallicTexture >= 0 &&
                  mat.roughnessTexture >= 0 && mat.occlusionTexture >= 0 &&
                  mat.emissiveTexture >= 0,
              "material fixture imports all six PBR channels");
        SaveAsset(material, root / "Material.plasset");
        auto materialRoundtrip =
            std::dynamic_pointer_cast<ModelAsset>(ReadAsset(root / "Material.plasset"));
        Check(materialRoundtrip->materials[0].normalTexture == mat.normalTexture &&
                  materialRoundtrip->materials[0].emissiveTexture == mat.emissiveTexture,
              "cooked v2 retains PBR texture bindings");
        AudioSystem disabled({false, false, 48000});
        Check(!disabled.Available(), "disabled audio remains explicit and nonfatal");
        std::cout << "Asset integration tests passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
