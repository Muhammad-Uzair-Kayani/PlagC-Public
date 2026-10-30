#include "pgcpch.h"
#include <PlagC/Assets/AssetImport.h>
#include <fstream>
#include <iomanip>
#include <string>

namespace PlagC {
namespace {
constexpr unsigned ReceiptVersion = 1;

void RequireTag(std::istream& input, const char* expected)
{
    std::string tag;
    if (!(input >> tag) || tag != expected)
        throw std::runtime_error(std::string("invalid asset import receipt: expected ") + expected);
}

std::string LowerExtension(std::filesystem::path path)
{
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}
}

std::filesystem::path ImportReceiptPath(const std::filesystem::path& cooked)
{
    auto path = cooked;
    path += ".import";
    return path;
}

void Validate(const AssetImportReceipt& receipt)
{
    if (receipt.source.empty() || receipt.cooked.empty())
        throw std::invalid_argument("asset import source and cooked paths are required");
    if (receipt.cooked.extension() != ".plasset")
        throw std::invalid_argument("cooked asset destination must use .plasset");

    const std::string extension = LowerExtension(receipt.source);
    if (receipt.kind == AssetImportKind::Model)
    {
        if (extension != ".gltf" && extension != ".glb")
            throw std::invalid_argument("model import supports glTF 2.0 .gltf/.glb sources");
        if (!std::isfinite(receipt.model.scale) || receipt.model.scale <= 0.0f ||
            receipt.model.scale > 100000.0f)
            throw std::invalid_argument("model import scale must be in (0, 100000]");
    }
    else if (extension != ".png" && extension != ".jpg" && extension != ".jpeg")
        throw std::invalid_argument("texture import supports PNG/JPEG sources");
}

void SaveAssetImportReceipt(const AssetImportReceipt& receipt)
{
    Validate(receipt);
    const auto path = ImportReceiptPath(receipt.cooked);
    if (const auto parent = path.parent_path(); !parent.empty())
        std::filesystem::create_directories(parent);
    auto temporary = path;
    temporary += ".tmp";

    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("failed to open asset import receipt for writing");
    output << "PGIMPORT " << ReceiptVersion << '\n';
    output << "KIND " << static_cast<unsigned>(receipt.kind) << '\n';
    output << "SOURCE " << std::quoted(receipt.source.generic_string()) << '\n';
    output << "COOKED " << std::quoted(receipt.cooked.generic_string()) << '\n';
    output << "MODEL " << receipt.model.scale << ' '
           << static_cast<unsigned>(receipt.model.coordinates) << ' '
           << (receipt.model.flipUVs ? 1 : 0) << ' '
           << static_cast<unsigned>(receipt.model.normals) << ' '
           << (receipt.model.tangents ? 1 : 0) << ' '
           << (receipt.model.materials ? 1 : 0) << ' '
           << (receipt.model.animations ? 1 : 0) << '\n';
    output << "TEXTURE " << (receipt.texture.flipVertical ? 1 : 0) << '\n';
    output << "END\n";
    output.flush();
    if (!output)
        throw std::runtime_error("failed while writing asset import receipt");
    output.close();

    std::error_code error;
#ifdef _WIN32
    std::filesystem::remove(path, error);
    error.clear();
#endif
    std::filesystem::rename(temporary, path, error);
    if (error)
    {
        std::filesystem::remove(temporary);
        throw std::runtime_error("failed to commit asset import receipt: " + error.message());
    }
}

AssetImportReceipt LoadAssetImportReceipt(const std::filesystem::path& cooked)
{
    std::ifstream input(ImportReceiptPath(cooked), std::ios::binary);
    if (!input)
        throw std::runtime_error("asset has no import receipt; import it once before reimporting");

    RequireTag(input, "PGIMPORT");
    unsigned version = 0;
    if (!(input >> version) || version != ReceiptVersion)
        throw std::runtime_error("unsupported asset import receipt version");

    AssetImportReceipt receipt;
    RequireTag(input, "KIND");
    unsigned kind = 0;
    if (!(input >> kind) || kind > static_cast<unsigned>(AssetImportKind::Texture))
        throw std::runtime_error("invalid asset import kind");
    receipt.kind = static_cast<AssetImportKind>(kind);

    std::string source, savedCooked;
    RequireTag(input, "SOURCE");
    if (!(input >> std::quoted(source)))
        throw std::runtime_error("invalid import source path");
    RequireTag(input, "COOKED");
    if (!(input >> std::quoted(savedCooked)))
        throw std::runtime_error("invalid cooked asset path");
    receipt.source = source;
    receipt.cooked = savedCooked;
    if (receipt.cooked.lexically_normal() != cooked.lexically_normal())
        throw std::runtime_error("import receipt cooked path does not match requested asset");

    RequireTag(input, "MODEL");
    unsigned coordinates = 0, flipUVs = 0, normals = 0, tangents = 0, materials = 0, animations = 0;
    if (!(input >> receipt.model.scale >> coordinates >> flipUVs >> normals >> tangents >> materials >> animations) ||
        coordinates > static_cast<unsigned>(ModelCoordinateImport::MirrorZToRightHanded) ||
        normals > static_cast<unsigned>(ModelNormalImport::RecalculateSmooth) || flipUVs > 1 || tangents > 1 ||
        materials > 1 || animations > 1)
        throw std::runtime_error("invalid model import settings");
    receipt.model.coordinates = static_cast<ModelCoordinateImport>(coordinates);
    receipt.model.flipUVs = flipUVs != 0;
    receipt.model.normals = static_cast<ModelNormalImport>(normals);
    receipt.model.tangents = tangents != 0;
    receipt.model.materials = materials != 0;
    receipt.model.animations = animations != 0;

    RequireTag(input, "TEXTURE");
    unsigned flipVertical = 0;
    if (!(input >> flipVertical) || flipVertical > 1)
        throw std::runtime_error("invalid texture import settings");
    receipt.texture.flipVertical = flipVertical != 0;
    RequireTag(input, "END");
    Validate(receipt);
    return receipt;
}

void CookImportedAsset(const AssetImportReceipt& receipt)
{
    Validate(receipt);
    if (!std::filesystem::is_regular_file(receipt.source))
        throw std::runtime_error("import source file does not exist: " + receipt.source.string());

    if (receipt.kind == AssetImportKind::Model)
    {
        auto asset = ImportModel(receipt.source, receipt.model);
        SaveAsset(asset, receipt.cooked);
    }
    else
    {
        auto asset = ImportTexture(receipt.source, receipt.texture);
        SaveAsset(asset, receipt.cooked);
    }
    SaveAssetImportReceipt(receipt);
}

} // namespace PlagC
