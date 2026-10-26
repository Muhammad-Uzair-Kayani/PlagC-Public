#include <PlagC/Assets/Asset.h>
#include <cmath>
#include <iostream>
#include "StaticNodeSelection.h"
int main(int argc, char **argv)
{
    try
    {
        if (argc == 3 && std::string(argv[1]) == "--inspect")
        {
            auto asset = PlagC::ReadAsset(argv[2]);
            auto model = std::dynamic_pointer_cast<PlagC::ModelAsset>(asset);
            if (!model)
                throw std::runtime_error("inspection requires a model asset");
            PlagC::Validate(*model);
            if (model->meshes.empty())
                throw std::runtime_error("cooked model contains no meshes");
            std::size_t vertices = 0, triangles = 0;
            for (const auto &mesh : model->meshes)
            {
                vertices += mesh.geometry.vertices.size();
                triangles += mesh.geometry.indices.size() / 3;
            }
            std::cout << "Decoded: " << model->meshes.size() << " meshes, " << vertices
                      << " vertices, " << triangles << " triangles, " << model->textures.size()
                      << " textures\n";
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "--demo")
        {
            auto model = PlagC::ImportModel("Assets/Source/Beacon.gltf");
            PlagC::SaveAsset(model, "Assets/Cooked/Beacon.plasset");
            PlagC::AudioClip clip;
            clip.channels = 1;
            clip.sampleRate = 48000;
            for (unsigned i = 0; i < 48000; ++i)
            {
                double t = double(i) / 48000;
                double envelope = std::sin(3.141592653589793 * t);
                clip.samples.push_back(
                    std::int16_t(6000 * envelope * std::sin(2 * 3.141592653589793 * 440 * t)));
            }
            PlagC::SaveAsset(clip, "Assets/Cooked/Chime.plasset");
            std::cout << "Cooked Beacon and Chime demo assets\n";
            return 0;
        }
        std::vector<std::string> nodes;
        if (argc > 3 && argc % 2 == 1)
            for (int i = 3; i + 1 < argc; i += 2)
            {
                if (std::string(argv[i]) != "--node")
                    throw std::invalid_argument("unknown cooker option");
                nodes.push_back(argv[i + 1]);
            }
        const bool selectNode = !nodes.empty();
        if (argc != 3 && !selectNode)
        {
            std::cerr << "Usage: AssetCooker source.gltf|source.glb|source.wav output.plasset "
                         "[--node static-node-name]\n    "
                         "   AssetCooker --demo\n   AssetCooker --inspect model.plasset\n";
            return 2;
        }
        std::filesystem::path source = argv[1];
        if (selectNode && source.extension() == ".wav")
            throw std::invalid_argument("node selection requires a glTF model");
        if (source.extension() == ".wav")
            PlagC::SaveAsset(PlagC::ImportWave(source), argv[2]);
        else
        {
            auto model = PlagC::ImportModel(source);
            if (selectNode)
                SelectStaticNodes(model, nodes);
            PlagC::SaveAsset(model, argv[2]);
            std::cout << model.meshes.size() << " meshes, " << model.skeleton.joints.size()
                      << " nodes, " << model.animations.size() << " clips\n";
            for (auto &warning : model.warnings)
                std::cout << "Warning: " << warning << '\n';
        }
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Asset cook failed: " << e.what() << '\n';
        return 1;
    }
}
