#include "pgcpch.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <string>

namespace PlagC::Editor {
namespace {

const char* KindName(AssetImportKind kind)
{
    return kind == AssetImportKind::Model ? "Model" : "Texture";
}

std::string CookedAssetKey(std::filesystem::path path)
{
    path = path.lexically_normal();
    const auto cookedRoot = std::filesystem::path("Assets") / "Cooked";
    auto relative = path.lexically_relative(cookedRoot);
    if (relative.empty() || *relative.begin() == "..")
        return {};
    return relative.generic_string();
}

void ReloadIfKnown(Engine& engine, const std::filesystem::path& cooked)
{
    const std::string key = CookedAssetKey(cooked);
    if (key.empty())
        return;
    const auto registry = engine.Assets().Registry();
    const bool loaded = std::any_of(registry.begin(), registry.end(), [&](const AssetInfo& info) {
        std::string path = info.metadata.path;
        std::replace(path.begin(), path.end(), '\\', '/');
        return path == key || path.ends_with("/" + key) || path.ends_with("://" + key);
    });
    if (!loaded)
        return;
    engine.Assets().Reload(key);
    engine.Assets().Wait();
}

AssetImportReceipt ReceiptFromUI(const EditorLayer&)
{
    return {};
}

} // namespace

void EditorLayer::AppendAssetImport(Engine& engine, EditorSession& session)
{
    if (!showAssetImport_)
        return;
    if (!ImGui::Begin("Asset Import", &showAssetImport_))
    {
        ImGui::End();
        return;
    }

    ImGui::TextDisabled("Source -> cooked PlagC asset");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##ImportSource", "Source .gltf/.glb/.png/.jpg",
                             assetImportSource_.data(), assetImportSource_.size());
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##ImportCooked", "Assets/Cooked/name.plasset",
                             assetImportCooked_.data(), assetImportCooked_.size());

    if (ImGui::BeginCombo("Asset Type", KindName(assetImportKind_)))
    {
        if (ImGui::Selectable("Model", assetImportKind_ == AssetImportKind::Model))
            assetImportKind_ = AssetImportKind::Model;
        if (ImGui::Selectable("Texture", assetImportKind_ == AssetImportKind::Texture))
            assetImportKind_ = AssetImportKind::Texture;
        ImGui::EndCombo();
    }

    if (assetImportKind_ == AssetImportKind::Model)
    {
        ImGui::SeparatorText("Model Import Settings");
        ImGui::DragFloat("Scale", &modelImportOptions_.scale, 0.01f, 0.0001f, 100000.0f, "%.4f");
        const char* coordinates = modelImportOptions_.coordinates == ModelCoordinateImport::RightHandedYUp
            ? "Right-handed Y-up"
            : "Mirror Z to right-handed";
        if (ImGui::BeginCombo("Coordinates", coordinates))
        {
            if (ImGui::Selectable("Right-handed Y-up",
                                  modelImportOptions_.coordinates == ModelCoordinateImport::RightHandedYUp))
                modelImportOptions_.coordinates = ModelCoordinateImport::RightHandedYUp;
            if (ImGui::Selectable("Mirror Z to right-handed",
                                  modelImportOptions_.coordinates == ModelCoordinateImport::MirrorZToRightHanded))
                modelImportOptions_.coordinates = ModelCoordinateImport::MirrorZToRightHanded;
            ImGui::EndCombo();
        }
        ImGui::Checkbox("Flip UV V", &modelImportOptions_.flipUVs);
        const char* normals = modelImportOptions_.normals == ModelNormalImport::PreserveOrGenerate
            ? "Preserve / generate if missing"
            : "Recalculate smooth";
        if (ImGui::BeginCombo("Normals", normals))
        {
            if (ImGui::Selectable("Preserve / generate if missing",
                                  modelImportOptions_.normals == ModelNormalImport::PreserveOrGenerate))
                modelImportOptions_.normals = ModelNormalImport::PreserveOrGenerate;
            if (ImGui::Selectable("Recalculate smooth",
                                  modelImportOptions_.normals == ModelNormalImport::RecalculateSmooth))
                modelImportOptions_.normals = ModelNormalImport::RecalculateSmooth;
            ImGui::EndCombo();
        }
        ImGui::Checkbox("Tangents", &modelImportOptions_.tangents);
        ImGui::Checkbox("Materials + Textures", &modelImportOptions_.materials);
        ImGui::Checkbox("Animations", &modelImportOptions_.animations);
        ImGui::TextDisabled("Supported model sources: glTF 2.0 (.gltf, .glb)");
    }
    else
    {
        ImGui::SeparatorText("Texture Import Settings");
        ImGui::Checkbox("Flip Vertical", &textureImportOptions_.flipVertical);
        ImGui::TextDisabled("Supported texture sources: PNG, JPEG. Cooked textures load through AssetManager.");
    }

    auto currentReceipt = [&] {
        AssetImportReceipt receipt;
        receipt.kind = assetImportKind_;
        receipt.source = assetImportSource_.data();
        receipt.cooked = assetImportCooked_.data();
        receipt.model = modelImportOptions_;
        receipt.texture = textureImportOptions_;
        return receipt;
    };
    auto publish = [&](const AssetImportReceipt& receipt, const char* verb) {
        try
        {
            CookImportedAsset(receipt);
            ReloadIfKnown(engine, receipt.cooked);
            dataPanels_.RefreshAssets(engine);
            assetImportError_.clear();
            assetImportStatus_ = std::string(verb) + " " + receipt.source.generic_string() +
                                 " -> " + receipt.cooked.generic_string();
            engine.Logger().Write(Severity::Info, "EDITOR-IMPORT", assetImportStatus_);
        }
        catch (const std::exception& error)
        {
            assetImportStatus_.clear();
            assetImportError_ = error.what();
            engine.Logger().Write(Severity::Error, "EDITOR-IMPORT", assetImportError_);
        }
    };

    ImGui::BeginDisabled(!session.EditingEnabled());
    if (ImGui::Button("Import / Cook"))
        publish(currentReceipt(), "Imported");
    ImGui::SameLine();
    if (ImGui::Button("Load Saved Settings"))
    {
        try
        {
            const auto receipt = LoadAssetImportReceipt(assetImportCooked_.data());
            assetImportKind_ = receipt.kind;
            modelImportOptions_ = receipt.model;
            textureImportOptions_ = receipt.texture;
            std::snprintf(assetImportSource_.data(), assetImportSource_.size(), "%s",
                          receipt.source.generic_string().c_str());
            std::snprintf(assetImportCooked_.data(), assetImportCooked_.size(), "%s",
                          receipt.cooked.generic_string().c_str());
            assetImportError_.clear();
            assetImportStatus_ = "Loaded saved source/import relationship.";
        }
        catch (const std::exception& error)
        {
            assetImportStatus_.clear();
            assetImportError_ = error.what();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reimport Saved"))
    {
        try
        {
            const auto receipt = LoadAssetImportReceipt(assetImportCooked_.data());
            publish(receipt, "Reimported");
        }
        catch (const std::exception& error)
        {
            assetImportStatus_.clear();
            assetImportError_ = error.what();
            engine.Logger().Write(Severity::Error, "EDITOR-IMPORT", assetImportError_);
        }
    }
    ImGui::EndDisabled();

    if (!assetImportStatus_.empty())
        ImGui::TextWrapped("%s", assetImportStatus_.c_str());
    if (!assetImportError_.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.35f, 0.30f, 1.0f));
        ImGui::TextWrapped("Import failed: %s", assetImportError_.c_str());
        ImGui::PopStyleColor();
    }
    if (!session.EditingEnabled())
        ImGui::TextDisabled("Asset import is locked while Play mode is running.");

    ImGui::End();
}

} // namespace PlagC::Editor
