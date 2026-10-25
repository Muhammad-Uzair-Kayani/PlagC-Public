#include <PlagC/Assets/Asset.h>
#include <cmath>
#include <iostream>
int main(int argc, char **argv)
{
    try
    {
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
        if (argc != 3)
        {
            std::cerr << "Usage: AssetCooker source.gltf|source.glb|source.wav output.plasset\n    "
                         "   AssetCooker --demo\n";
            return 2;
        }
        std::filesystem::path source = argv[1];
        if (source.extension() == ".wav")
            PlagC::SaveAsset(PlagC::ImportWave(source), argv[2]);
        else
        {
            auto model = PlagC::ImportModel(source);
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
