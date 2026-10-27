#include <PlagC/Assets/Asset.h>
#include <PlagC/Animation/Animation.h>
#include <cmath>
#include <iostream>
#include <set>
int main()
{
    using namespace PlagC;
    try
    {
        auto asset =
            std::dynamic_pointer_cast<ModelAsset>(ReadAsset("Assets/Cooked/TPS/Human.plasset"));
        if (!asset)
            throw std::runtime_error("human model missing");
        Validate(*asset);
        AssetHandle<ModelAsset> handle(asset);
        Animator animator(handle);
        std::set<std::string> names;
        for (unsigned clip = 0; clip < asset->animations.size(); ++clip)
        {
            names.insert(asset->animations[clip].name);
            for (float ratio : {0.f, .25f, .5f, .75f})
            {
                AnimationState state;
                state.clip = clip;
                state.time = asset->animations[clip].duration * ratio;
                animator.Update(state, 0);
                for (unsigned mesh = 0; mesh < asset->meshes.size(); ++mesh)
                    for (auto v : animator.Deform(mesh).vertices)
                        if (!std::isfinite(v.position.x) || !std::isfinite(v.position.y) ||
                            !std::isfinite(v.position.z) || std::abs(v.position.x) > 4 ||
                            std::abs(v.position.y) > 4 || std::abs(v.position.z) > 4)
                            throw std::runtime_error("invalid human deformation: " +
                                                     asset->animations[clip].name);
            }
        }
        for (const char *name : {"Idle",
                                 "Walk",
                                 "Run",
                                 "Sprint",
                                 "Aim",
                                 "Fire",
                                 "Reload",
                                 "Jump",
                                 "Fall",
                                 "Land",
                                 "Hit",
                                 "Death",
                                 "AimWalk",
                                 "AimBack",
                                 "AimLeft",
                                 "AimRight",
                                 "AimForwardLeft",
                                 "AimForwardRight",
                                 "AimBackLeft",
                                 "AimBackRight"})
            if (!names.contains(name))
                throw std::runtime_error(std::string("Missing clip: ") + name);
        bool socket = false;
        for (const auto &node : asset->skeleton.joints)
            if (node.name == "WeaponSocket")
                socket = true;
        if (!socket || asset->textures.empty())
            throw std::runtime_error("human lost weapon socket or textures");
        std::cout << "[PASS] " << names.size()
                  << " human clips deform correctly at four sampled poses; weapon socket and "
                     "textures preserved\n";
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
