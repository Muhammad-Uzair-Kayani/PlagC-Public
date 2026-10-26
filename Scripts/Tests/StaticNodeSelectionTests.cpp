#include <PlagC/Assets/Asset.h>
#include "../../Tools/AssetCooker/StaticNodeSelection.h"
#include <iostream>
int main()
{
    try
    {
        PlagC::ModelAsset model;
        model.skeleton.joints = {{"root", -1, {}}, {"selected", 0, {}}, {"unselected", 0, {}}};
        model.skeleton.joints[1].local.translation = {5, 2, 1};
        for (int i = 0; i < 3; ++i)
        {
            PlagC::TextureAsset t;
            t.width = t.height = 1;
            t.rgba = {255, 255, 255, 255};
            model.textures.push_back(t);
            PlagC::MaterialAsset m;
            m.baseColourTexture = i == 1 ? 0 : i;
            model.materials.push_back(m);
            PlagC::MeshAsset mesh;
            mesh.node = i == 2 ? 2 : 1;
            mesh.material = unsigned(i);
            mesh.geometry = PlagC::MakePrimitive(PlagC::Primitive::Cube);
            model.meshes.push_back(mesh);
        }
        auto original = model;
        SelectStaticNode(model, "selected");
        if (model.meshes.size() != 2 || model.materials.size() != 2 || model.textures.size() != 1)
            throw std::runtime_error("selection or texture sharing/pruning failed");
        if (model.skeleton.joints[1].local.translation.x != 5)
            throw std::runtime_error("bind transform changed");
        PlagC::Validate(model);
        bool rejected = false;
        try
        {
            SelectStaticNode(original, "missing");
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error("missing node accepted");
        original.animations.emplace_back();
        rejected = false;
        try
        {
            SelectStaticNode(original, "selected");
        }
        catch (const std::invalid_argument &)
        {
            rejected = true;
        }
        if (!rejected)
            throw std::runtime_error("animated selection accepted");
        std::cout << "PASS: static node selection retains bind transforms, prunes unused "
                     "resources, shares textures, and rejects invalid/animated selections\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
