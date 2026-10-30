#pragma once

#include <PlagC/Assets/Asset.h>
#include <filesystem>

namespace PlagC {

enum class AssetImportKind : std::uint8_t
{
    Model,
    Texture
};

struct AssetImportReceipt
{
    AssetImportKind kind = AssetImportKind::Model;
    std::filesystem::path source;
    std::filesystem::path cooked;
    ModelImportOptions model{};
    TextureImportOptions texture{};
};

std::filesystem::path ImportReceiptPath(const std::filesystem::path& cooked);
void Validate(const AssetImportReceipt& receipt);
void SaveAssetImportReceipt(const AssetImportReceipt& receipt);
AssetImportReceipt LoadAssetImportReceipt(const std::filesystem::path& cooked);
void CookImportedAsset(const AssetImportReceipt& receipt);

} // namespace PlagC
