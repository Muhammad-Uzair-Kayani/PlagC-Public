#include <PlagC/Animation/Animation.h>
#include "../../Game/src/WeaponAnimation.h"
#include <iostream>
#include <stdexcept>
using namespace PlagC;
void Check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
}
float Difference(const Matrix4 &a, const Matrix4 &b)
{
    float d = 0;
    for (unsigned k = 0; k < 16; ++k)
        d = std::max(d, std::abs(a.m[k] - b.m[k]));
    return d;
}
void TestRig(const char *path, const char *rootName)
{
    auto asset = std::dynamic_pointer_cast<ModelAsset>(ReadAsset(path));
    Check(bool(asset), "missing model");
    auto clip = [&](const char *name) {
        for (unsigned i = 0; i < asset->animations.size(); ++i)
            if (asset->animations[i].name == name)
                return i;
        throw std::runtime_error(name);
    };
    unsigned root = 0;
    while (root < asset->skeleton.joints.size() && asset->skeleton.joints[root].name != rootName)
        ++root;
    Check(root < asset->skeleton.joints.size(), "missing spine");
    std::vector<const char *> movements{"Idle", "Walk", "Run"};
    if (std::string(rootName) == "spine_01")
        movements.insert(movements.end(),
                         {"Sprint", "AimWalk", "AimRun", "AimBack", "AimLeft", "AimRight"});
    for (const char *movement : movements)
    {
        Animator plain{AssetHandle<ModelAsset>(asset)}, layered{AssetHandle<ModelAsset>(asset)};
        AnimatorComponent base, state;
        base.state.clip = clip(movement);
        state = base;
        state.upperBody = AnimationUpperBody{{clip("Aim"), 0, 0, false, false}, rootName, 1, 0};
        Game::WeaponAnimation weapon;
        for (unsigned frame = 0; frame < 180; ++frame)
        {
            // Alternate ADS while firing: no weapon lowering or base clock resets.
            weapon.Update(1. / 60, frame % 60 < 30, true, frame % 9 == 0, false);
            state.upperBody->weight = weapon.weight;
            state.upperBody->recoilRadians = weapon.Recoil();
            plain.Update(base, 1. / 60);
            layered.Update(state, 1. / 60);
            Check(std::abs(state.state.time - base.state.time) < 1e-8,
                  "firing reset locomotion clock");
            for (unsigned i = 0; i < asset->skeleton.joints.size(); ++i)
            {
                bool masked = false;
                for (int j = int(i); j >= 0; j = asset->skeleton.joints[j].parent)
                    masked |= unsigned(j) == root;
                if (!masked)
                    Check(Difference(plain.Pose()[i], layered.Pose()[i]) < 2e-4,
                          "weapon moved lower body");
                for (float v : layered.Pose()[i].m)
                    Check(std::isfinite(v), "nonfinite pose");
            }
            if (frame > 10)
                Check(weapon.weight == 1, "burst lowered weapon");
        }
        // Verify that the layer really affects the weapon chain, not just that legs survive.
        state.upperBody->weight = 1;
        state.upperBody->recoilRadians = 0;
        layered.Update(state, 0);
        auto readySpine = layered.Pose()[root];
        state.upperBody->recoilRadians = -.035f;
        layered.Update(state, 0);
        Check(Difference(readySpine, layered.Pose()[root]) > .001f,
              "recoil did not affect the weapon chain");
        // Reload must preserve locomotion and crossfade from the displayed weapon pose.
        unsigned actionIndex = 0;
        for (unsigned action : {clip("Reload"), clip("Aim"), clip("Reload"), clip("Aim")})
        {
            auto beforeAction = std::vector<Matrix4>(layered.Pose().begin(), layered.Pose().end());
            state.upperBody->pose.clip = action;
            state.upperBody->pose.time = 0;
            state.upperBody->recoilRadians = 0;
            layered.Update(state, 0);
            for (unsigned i = 0; i < beforeAction.size(); ++i)
                Check(Difference(beforeAction[i], layered.Pose()[i]) < 2e-4,
                      "weapon action transition popped");
            // Complete one reload, then deliberately interrupt subsequent fades.
            const unsigned frames = actionIndex++ == 0 ? 126 : 4;
            for (unsigned frame = 0; frame < frames; ++frame)
            {
                state.upperBody->pose.time =
                    action == clip("Reload")
                        ? (frame + 1) / 126. * asset->animations[action].duration
                        : 0;
                plain.Update(base, 1. / 60);
                layered.Update(state, 1. / 60);
                for (unsigned i = 0; i < asset->skeleton.joints.size(); ++i)
                {
                    bool masked = false;
                    for (int j = int(i); j >= 0; j = asset->skeleton.joints[j].parent)
                        masked |= unsigned(j) == root;
                    if (!masked)
                        Check(Difference(plain.Pose()[i], layered.Pose()[i]) < 2e-4,
                              "reload changed locomotion");
                }
            }
        }
        weapon.Update(.05, false, false, false, false);
        Check(weapon.ready, "release lost hold grace");
        weapon.Update(.5, false, false, false, false);
        Check(weapon.weight == 0, "weapon did not lower");
        weapon.Update(.2, true, true, true, false);
        weapon.Update(.2, true, true, false, true);
        Check(!weapon.ready && weapon.weight == 0 && weapon.Recoil() == 0,
              "override did not suppress weapon");
    }
    AnimationStateMachine graph;
    graph.parameters.push_back({"Mode", AnimationParameterType::Float, 0});
    graph.states = {{"Idle", clip("Idle")}, {"Walk", clip("Walk")}, {"Run", clip("Run")}};
    graph.transitions = {{0, 1, .2f, {{"Mode", AnimationCompare::Equal, 1}}},
                         {1, 2, .2f, {{"Mode", AnimationCompare::Equal, 2}}}};
    Animator animator{AssetHandle<ModelAsset>(asset)};
    AnimatorComponent state;
    state.controller.emplace(graph, unsigned(asset->animations.size()));
    animator.Update(state, .01);
    state.controller->SetFloat("Mode", 1);
    animator.Update(state, .05);
    auto before = std::vector<Matrix4>(animator.Pose().begin(), animator.Pose().end());
    state.controller->SetFloat("Mode", 2);
    animator.Update(state, 0);
    Check(state.controller->Current().name == "Run", "transition waited for old fade");
    for (unsigned i = 0; i < before.size(); ++i)
        Check(Difference(before[i], animator.Pose()[i]) < 2e-4, "interrupted fade popped");
    std::cout
        << path
        << ": masking, repeated shots, aim switching, overrides and interrupted fades passed\n";
}
int main(int argc, char **argv)
{
    try
    {
        TestRig("Assets/Cooked/TPS/Operative.plasset", "spine");
        if (argc > 1)
            TestRig(argv[1], "spine_01");
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
