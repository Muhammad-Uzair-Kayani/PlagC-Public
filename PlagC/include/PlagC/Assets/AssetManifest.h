#pragma once

#include <PlagC/Assets/AssetManager.h>
#include <filesystem>
#include <string>
#include <vector>

namespace PlagC
{
struct AssetManifestEntry
{
    std::string path;
    AssetType type = AssetType::Model;
};

class AssetManifest
{
  public:
    static AssetManifest Load(const std::filesystem::path &file);

    const std::string &Name() const { return name_; }
    const std::vector<AssetManifestEntry> &Assets() const { return assets_; }

    void LoadAll(AssetManager &assets) const;
    void UnloadAll(AssetManager &assets) const;

  private:
    std::string name_;
    std::vector<AssetManifestEntry> assets_;
};
} // namespace PlagC
