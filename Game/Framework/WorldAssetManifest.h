#pragma once

#include <PlagC/Assets/AssetManager.h>
#include <filesystem>
#include <string>
#include <vector>

namespace PlagCGame
{
struct WorldAssetEntry
{
    std::string path;
    PlagC::AssetType type = PlagC::AssetType::Model;
};

class WorldAssetManifest
{
  public:
    static WorldAssetManifest Load(const std::filesystem::path &file);

    const std::string &World() const { return world_; }
    const std::vector<WorldAssetEntry> &Assets() const { return assets_; }

    void LoadAll(PlagC::AssetManager &assets) const;
    void UnloadAll(PlagC::AssetManager &assets) const;

  private:
    std::string world_;
    std::vector<WorldAssetEntry> assets_;
};
} // namespace PlagCGame
