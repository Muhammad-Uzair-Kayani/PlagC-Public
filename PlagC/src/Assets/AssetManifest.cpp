#include "pgcpch.h"
#include <PlagC/Assets/AssetManifest.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace PlagC
{
namespace
{
AssetType ParseType(const std::string &type)
{
    if (type == "Model")
        return AssetType::Model;
    if (type == "Audio")
        return AssetType::Audio;
    throw std::invalid_argument("unknown asset type in manifest: " + type);
}
} // namespace

AssetManifest AssetManifest::Load(const std::filesystem::path &file)
{
    std::ifstream input(file, std::ios::binary);
    if (!input)
        throw std::runtime_error("asset manifest not found: " + file.string());

    std::ostringstream buffer;
    buffer << input.rdbuf();

    const std::string text = buffer.str();
    rapidjson::Document document;
    document.Parse(text.c_str());
    if (document.HasParseError())
        throw std::runtime_error(
            "invalid asset manifest " + file.string() + ": " +
            rapidjson::GetParseError_En(document.GetParseError()) + " at offset " +
            std::to_string(document.GetErrorOffset()));

    if (!document.IsObject() || !document.HasMember("assets") ||
        !document["assets"].IsArray())
        throw std::runtime_error("asset manifest requires an 'assets' array: " +
                                 file.string());

    AssetManifest manifest;
    if (document.HasMember("name"))
    {
        if (!document["name"].IsString())
            throw std::runtime_error("asset manifest 'name' must be a string: " +
                                     file.string());
        manifest.name_ = document["name"].GetString();
    }

    for (const auto &value : document["assets"].GetArray())
    {
        AssetManifestEntry entry;
        if (value.IsString())
        {
            entry.path = value.GetString();
        }
        else if (value.IsObject() && value.HasMember("path") &&
                 value["path"].IsString())
        {
            entry.path = value["path"].GetString();
            if (value.HasMember("type"))
            {
                if (!value["type"].IsString())
                    throw std::runtime_error(
                        "asset manifest type must be a string: " + file.string());
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

void AssetManifest::LoadAll(AssetManager &assets) const
{
    for (const auto &entry : assets_)
        assets.Request(entry.path, entry.type);

    assets.Wait();

    const auto registry = assets.Registry();
    for (const auto &entry : assets_)
    {
        const auto found =
            std::find_if(registry.begin(), registry.end(), [&](const AssetInfo &info) {
                return info.metadata.path == entry.path;
            });
        if (found == registry.end())
            throw std::runtime_error("asset was not registered: " + entry.path);
        if (found->state != AssetState::Ready)
            throw std::runtime_error("failed to load asset '" + entry.path + "': " +
                                     found->error);
    }
}

void AssetManifest::UnloadAll(AssetManager &assets) const
{
    for (auto it = assets_.rbegin(); it != assets_.rend(); ++it)
        assets.Unload(it->path);
}
} // namespace PlagC
