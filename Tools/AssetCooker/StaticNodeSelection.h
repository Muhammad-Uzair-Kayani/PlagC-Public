#pragma once
#include <PlagC/Assets/Asset.h>
#include <algorithm>
#include <unordered_map>
#include <stdexcept>
#include <utility>
// Modular glTF downloads are catalogs, not assembled scenes. Select an explicitly
// named static node/subtree while retaining its bind transform and material maps.
inline void SelectStaticNodes(PlagC::ModelAsset &model, const std::vector<std::string> &names)
{
    if (!model.animations.empty())
        throw std::invalid_argument("node selection requires a static model");
    if (names.empty())
        throw std::invalid_argument("node selection is empty");
    std::vector<int> selected;
    for (const auto &name : names)
    {
        auto found = std::find_if(model.skeleton.joints.begin(), model.skeleton.joints.end(),
                                  [&](const auto &n) { return n.name == name; });
        if (found == model.skeleton.joints.end())
            throw std::invalid_argument("static node not found: " + name);
        if (std::count_if(model.skeleton.joints.begin(), model.skeleton.joints.end(),
                          [&](const auto &n) { return n.name == name; }) != 1)
            throw std::invalid_argument("static node name is ambiguous: " + name);
        selected.push_back(int(found - model.skeleton.joints.begin()));
    }
    auto included = [&](int index) {
        return std::find(selected.begin(), selected.end(), index) != selected.end();
    };
    std::vector<PlagC::MeshAsset> meshes;
    for (auto &mesh : model.meshes)
    {
        int node = int(mesh.node);
        while (node >= 0 && !included(node))
            node = model.skeleton.joints.at(unsigned(node)).parent;
        if (!included(node))
            continue;
        if (!mesh.skin.empty())
            throw std::invalid_argument("node selection does not support skinned meshes");
        meshes.push_back(std::move(mesh));
    }
    if (meshes.empty())
        throw std::invalid_argument("selected static nodes have no meshes");
    std::vector<PlagC::MaterialAsset> materials;
    std::vector<PlagC::TextureAsset> textures;
    std::unordered_map<unsigned, unsigned> materialMap, textureMap;
    auto texture = [&](int &index) {
        if (index < 0)
            return;
        auto old = unsigned(index);
        auto [it, inserted] = textureMap.emplace(old, unsigned(textures.size()));
        if (inserted)
            textures.push_back(std::move(model.textures.at(old)));
        index = int(it->second);
    };
    for (auto &mesh : meshes)
    {
        auto [it, inserted] = materialMap.emplace(mesh.material, unsigned(materials.size()));
        if (inserted)
        {
            auto material = model.materials.at(mesh.material);
            texture(material.baseColourTexture);
            texture(material.normalTexture);
            texture(material.metallicTexture);
            texture(material.roughnessTexture);
            texture(material.occlusionTexture);
            texture(material.emissiveTexture);
            materials.push_back(std::move(material));
        }
        mesh.material = it->second;
    }
    model.meshes = std::move(meshes);
    model.materials = std::move(materials);
    model.textures = std::move(textures);
    PlagC::Validate(model);
}

inline void SelectStaticNode(PlagC::ModelAsset &model, const std::string &name)
{
    SelectStaticNodes(model, {name});
}
