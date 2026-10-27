#include "pgcpch.h"
#include "WorldAssetManifest.h"
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace PlagCGame
{
namespace
{
PlagC::AssetType ParseType(const std::string &type)
{
    if (type == "Model")
        return PlagC::AssetType::Model;
    if (type == "Audio")
        return PlagC::AssetType::Audio;
    throw std::invalid_argument("unknown asset type in manifest: " + type);
}
}

WorldAssetManifest WorldAssetManifest::Load(const std::filesystem::path &file)
{
    std::ifstream input(file, std::ios::binary);
    if (!input)
        throw std::runtime_error("world asset manifest not found: " + file.string());

    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string text = buffer.str();

    rapidjson::Document doc;
    doc.Parse(text.c_str());
    if (doc.HasParseError())
        throw std::runtime_error("invalid world asset manifest " + file.string() + ": " +
                                 rapidjson::GetParseError_En(doc.GetParseError()) +
                                 " at offset " + std::to_string(doc.GetErrorOffset()));

    if (!doc.IsObject() || !doc.HasMember("world") || !doc["world"].IsString() ||
        !doc.HasMember("assets") || !doc["assets"].IsArray())
        throw std::runtime_error("world asset manifest requires string 'world' and array 'assets': " +
                                 file.string());

    WorldAssetManifest manifest;
    manifest.world_ = doc["world"].GetString();

    for (const auto &value : doc["assets"].GetArray())
    {
        WorldAssetEntry entry;
        if (value.IsString())
        {
            entry.path = value.GetString();
            entry.type = PlagC::AssetType::Model;
        }
        else if (value.IsObject() && value.HasMember("path") && value["path"].IsString())
        {
            entry.path = value["path"].GetString();
            if (value.HasMember("type"))
            {
                if (!value["type"].IsString())
                    throw std::runtime_error("asset manifest type must be a string: " + file.string());
                entry.type = ParseType(value["type"].GetString());
            }
        }
        else
        {
            throw std::runtime_error("invalid asset entry in manifest: " + file.string());
        }

        if (entry.path.empty())
            throw std::runtime_error("empty asset path in manifest: " + file.string());
        manifest.assets_.push_back(std::move(entry));
    }

    return manifest;
}

void WorldAssetManifest::LoadAll(PlagC::AssetManager &assets) const
{
    for (const auto &entry : assets_)
        assets.Request(entry.path, entry.type);

    assets.Wait();

    const auto registry = assets.Registry();
    for (const auto &entry : assets_)
    {
        auto it = std::find_if(registry.begin(), registry.end(), [&](const PlagC::AssetInfo &info) {
            return info.metadata.path == entry.path;
        });
        if (it == registry.end())
            throw std::runtime_error("asset was not registered: " + entry.path);
        if (it->state != PlagC::AssetState::Ready)
            throw std::runtime_error("failed to load asset '" + entry.path + "': " + it->error);
    }
}

void WorldAssetManifest::UnloadAll(PlagC::AssetManager &assets) const
{
    for (auto it = assets_.rbegin(); it != assets_.rend(); ++it)
        assets.Unload(it->path);
}
} // namespace PlagCGame
