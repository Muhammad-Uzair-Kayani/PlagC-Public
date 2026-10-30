#include "pgcpch.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <filesystem>
#include <string>

namespace PlagC::Editor {
namespace {

struct MaterialEditorState
{
    std::uint64_t entity = 0;
    int slot = -1;
    std::shared_ptr<const MaterialAsset> source;
    MaterialAsset draft;
    std::array<char, 384> path{};
    std::array<char, 384> baseColour{};
    std::array<char, 384> normal{};
    std::array<char, 384> metallic{};
    std::array<char, 384> roughness{};
    std::array<char, 384> occlusion{};
    std::array<char, 384> emissive{};
    std::string status;
    bool dirty = false;
};

void LogMaterialError(Engine& engine, const std::exception& error)
{
    engine.Logger().Write(Severity::Error, "EDITOR-MATERIAL", error.what());
}

void Copy(std::array<char, 384>& destination, const std::string& source)
{
    std::snprintf(destination.data(), destination.size(), "%s", source.c_str());
}

std::string SafeMaterialName(std::string name, std::size_t slot)
{
    if (name.empty())
        name = "Material_" + std::to_string(slot);
    for (char& c : name)
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_')
            c = '_';
    return name;
}

std::string LogicalMaterialPath(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    constexpr std::string_view mounted = "Assets://Cooked/";
    constexpr std::string_view disk = "Assets/Cooked/";
    constexpr std::string_view cooked = "Cooked/";
    if (path.starts_with(mounted)) path.erase(0, mounted.size());
    else if (path.starts_with(disk)) path.erase(0, disk.size());
    else if (path.starts_with(cooked)) path.erase(0, cooked.size());

    if (path.empty() || path.front() == '/' || path.find(':') != std::string::npos)
        throw std::invalid_argument("Material asset path must be relative to Assets/Cooked");
    for (const auto& part : std::filesystem::path(path))
        if (part == "..")
            throw std::invalid_argument("Material asset path traversal is not allowed");
    if (std::filesystem::path(path).extension() != ".pgmaterial")
        throw std::invalid_argument("Material assets must use the .pgmaterial extension");
    return path;
}

std::string DefaultMaterialPath(const MaterialAsset& imported, std::size_t slot)
{
    return "Assets/Cooked/Materials/" + SafeMaterialName(imported.name, slot) + ".pgmaterial";
}

MaterialAsset StandaloneFromImported(const MaterialAsset& imported, const std::string& path)
{
    MaterialAsset material;
    material.name = imported.name.empty() ? "Material" : imported.name;
    material.assetPath = LogicalMaterialPath(path);
    material.colour = imported.colour;
    material.roughness = imported.roughness;
    material.metallic = imported.metallic;
    material.emissive = imported.emissive;
    material.normalScale = imported.normalScale;
    material.alphaCutoff = imported.alphaCutoff;
    material.doubleSided = imported.doubleSided;
    return material;
}

void PullTexturePaths(MaterialEditorState& state)
{
    state.draft.baseColourTexturePath = state.baseColour.data();
    state.draft.normalTexturePath = state.normal.data();
    state.draft.metallicTexturePath = state.metallic.data();
    state.draft.roughnessTexturePath = state.roughness.data();
    state.draft.occlusionTexturePath = state.occlusion.data();
    state.draft.emissiveTexturePath = state.emissive.data();
}

void ValidateTextureFiles(const MaterialAsset& material)
{
    auto require = [](const std::string& resource, const char* label)
    {
        if (resource.empty())
            return;
        const auto path = std::filesystem::path("Assets") / resource;
        if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path))
            throw std::invalid_argument(std::string(label) + " texture does not exist under Assets: " + resource);
    };
    require(material.baseColourTexturePath, "Base-colour");
    require(material.normalTexturePath, "Normal");
    require(material.metallicTexturePath, "Metallic");
    require(material.roughnessTexturePath, "Roughness");
    require(material.occlusionTexturePath, "AO");
    require(material.emissiveTexturePath, "Emissive");
}

AssetHandle<MaterialAsset> PreviewHandle(MaterialAsset material, const std::string& path)
{
    material.assetPath = LogicalMaterialPath(path);
    Validate(material);
    ValidateTextureFiles(material);
    return AssetHandle<MaterialAsset>(std::make_shared<MaterialAsset>(std::move(material)));
}

void SyncState(MaterialEditorState& state, const AuthoringEntity& selected, int slot)
{
    const auto& model = *selected.record.model;
    const auto& imported = model.asset->materials.at(static_cast<std::size_t>(slot));
    std::shared_ptr<const MaterialAsset> source;
    if (static_cast<std::size_t>(slot) < model.materialOverrides.size())
        source = model.materialOverrides[slot].Shared();

    if (state.entity == selected.persistentID && state.slot == slot && state.source == source)
        return;

    state = {};
    state.entity = selected.persistentID;
    state.slot = slot;
    state.source = source;
    if (source)
    {
        state.draft = *source;
        const std::string fullPath = "Assets/Cooked/" + source->assetPath;
        Copy(state.path, fullPath);
    }
    else
    {
        const std::string path = DefaultMaterialPath(imported, static_cast<std::size_t>(slot));
        state.draft = StandaloneFromImported(imported, path);
        Copy(state.path, path);
    }
    Copy(state.baseColour, state.draft.baseColourTexturePath);
    Copy(state.normal, state.draft.normalTexturePath);
    Copy(state.metallic, state.draft.metallicTexturePath);
    Copy(state.roughness, state.draft.roughnessTexturePath);
    Copy(state.occlusion, state.draft.occlusionTexturePath);
    Copy(state.emissive, state.draft.emissiveTexturePath);
}

bool BeginMaterialProperties()
{
    constexpr ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp |
                                      ImGuiTableFlags_BordersInnerV |
                                      ImGuiTableFlags_PadOuterX;
    if (!ImGui::BeginTable("MaterialPBSProperties", 2, flags))
        return false;
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 112.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    return true;
}

void MaterialProperty(const char* name, const char* help)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", name);
    if (help && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", help);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

} // namespace

void EditorLayer::DrawMaterialInspector(Engine& engine, EditorSession& session)
{
    if (!showInspector_)
        return;

    const auto snapshot = session.ViewDocument().Snapshot();
    const auto selected = std::find_if(snapshot.begin(), snapshot.end(),
        [](const AuthoringEntity& entity) { return entity.selected; });
    if (selected == snapshot.end() || !selected->record.model ||
        !selected->record.model->asset || selected->record.model->asset->materials.empty())
        return;

    if (!ImGui::Begin("Inspector"))
    {
        ImGui::End();
        return;
    }

    static MaterialEditorState state;
    static int selectedSlot = 0;
    const auto& model = *selected->record.model;
    const auto& asset = *model.asset;
    selectedSlot = std::clamp(selectedSlot, 0, static_cast<int>(asset.materials.size() - 1));
    SyncState(state, *selected, selectedSlot);

    const bool editable = session.EditingEnabled();
    const bool locked = hierarchy_.Locked(selected->persistentID);
    const bool readOnly = !editable || locked;
    const bool hasOverride = static_cast<std::size_t>(selectedSlot) < model.materialOverrides.size() &&
                             bool(model.materialOverrides[selectedSlot]);

    ImGui::SeparatorText("Material System");
    if (ImGui::BeginCombo("Material Slot",
        asset.materials[selectedSlot].name.empty() ? "<unnamed>" : asset.materials[selectedSlot].name.c_str()))
    {
        for (std::size_t i = 0; i < asset.materials.size(); ++i)
        {
            const auto& material = asset.materials[i];
            const std::string label = std::to_string(i) + ": " +
                (material.name.empty() ? "<unnamed>" : material.name);
            if (ImGui::Selectable(label.c_str(), selectedSlot == static_cast<int>(i)))
            {
                selectedSlot = static_cast<int>(i);
                state.entity = 0;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::TextDisabled(hasOverride
        ? "Standalone .pgmaterial override — live preview uses this slot."
        : "Imported model material — editing creates a standalone slot override.");
    if (readOnly)
        ImGui::TextDisabled(editable ? "Hierarchy lock: material authoring is read-only."
                                     : "Play mode: material authoring is read-only.");

    ImGui::BeginDisabled(readOnly);
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputTextWithHint("##MaterialAssetPath", "Assets/Cooked/Materials/Example.pgmaterial",
                             state.path.data(), state.path.size());

    bool changed = false;
    if (BeginMaterialProperties())
    {
        MaterialProperty("Base Colour", "Linear colour factor multiplied by the optional sRGB base-colour texture.");
        changed |= ImGui::ColorEdit3("##BaseColour", &state.draft.colour.x);

        MaterialProperty("Base Texture", "sRGB colour texture path relative to Assets.");
        changed |= ImGui::InputText("##BaseColourTexture", state.baseColour.data(), state.baseColour.size());

        MaterialProperty("Normal Map", "Linear tangent-space normal map path relative to Assets.");
        changed |= ImGui::InputText("##NormalTexture", state.normal.data(), state.normal.size());

        MaterialProperty("Normal Scale", "Normal-map strength.");
        changed |= ImGui::DragFloat("##NormalScale", &state.draft.normalScale, 0.01f, 0.0f, 4.0f);

        MaterialProperty("Metallic", "PBS metallic factor from 0 to 1.");
        changed |= ImGui::SliderFloat("##Metallic", &state.draft.metallic, 0.0f, 1.0f);

        MaterialProperty("Metallic Map", "Linear metallic data map path relative to Assets.");
        changed |= ImGui::InputText("##MetallicTexture", state.metallic.data(), state.metallic.size());

        MaterialProperty("Roughness", "PBS roughness factor from 0 to 1.");
        changed |= ImGui::SliderFloat("##Roughness", &state.draft.roughness, 0.0f, 1.0f);

        MaterialProperty("Roughness Map", "Linear roughness data map path relative to Assets.");
        changed |= ImGui::InputText("##RoughnessTexture", state.roughness.data(), state.roughness.size());

        MaterialProperty("AO Map", "Linear ambient-occlusion data map path relative to Assets.");
        changed |= ImGui::InputText("##OcclusionTexture", state.occlusion.data(), state.occlusion.size());

        MaterialProperty("Emissive", "Linear emissive colour factor.");
        changed |= ImGui::ColorEdit3("##Emissive", &state.draft.emissive.x);

        MaterialProperty("Emissive Map", "sRGB emissive colour texture path relative to Assets.");
        changed |= ImGui::InputText("##EmissiveTexture", state.emissive.data(), state.emissive.size());

        MaterialProperty("Alpha Cutoff", "0 disables cutout; positive values enable alpha testing.");
        changed |= ImGui::SliderFloat("##AlphaCutoff", &state.draft.alphaCutoff, 0.0f, 1.0f);

        MaterialProperty("Double Sided", "Enables Ogre PBS two-sided lighting for this material.");
        changed |= ImGui::Checkbox("##DoubleSided", &state.draft.doubleSided);
        ImGui::EndTable();
    }

    if (changed)
    {
        try
        {
            PullTexturePaths(state);
            auto preview = PreviewHandle(state.draft, state.path.data());
            session.EditDocument().SetModelMaterial(
                selected->entity, static_cast<std::size_t>(selectedSlot), preview);
            state.draft = *preview;
            state.source = preview.Shared();
            state.dirty = true;
            state.status = "Live viewport preview active. Save Material to persist the .pgmaterial asset.";
        }
        catch (const std::exception& error)
        {
            state.status = error.what();
            LogMaterialError(engine, error);
        }
    }

    if (ImGui::Button("Save Material"))
    {
        try
        {
            PullTexturePaths(state);
            state.draft.assetPath = LogicalMaterialPath(state.path.data());
            ValidateTextureFiles(state.draft);
            SaveMaterialAsset(state.draft, state.path.data());
            auto material = LoadMaterialAsset(state.path.data());
            session.EditDocument().SetModelMaterial(
                selected->entity, static_cast<std::size_t>(selectedSlot), material);
            state.draft = *material;
            state.source = material.Shared();
            state.dirty = false;
            state.status = "Material saved and assigned to the selected slot.";
        }
        catch (const std::exception& error)
        {
            state.status = error.what();
            LogMaterialError(engine, error);
        }
    }

    ImGui::SameLine();
    ImGui::BeginDisabled(!hasOverride && !std::filesystem::exists(state.path.data()));
    if (ImGui::Button("Reload"))
    {
        try
        {
            auto material = LoadMaterialAsset(state.path.data());
            ValidateTextureFiles(*material);
            session.EditDocument().SetModelMaterial(
                selected->entity, static_cast<std::size_t>(selectedSlot), material);
            state.entity = 0;
            state.dirty = false;
            state.status = "Material reloaded from disk.";
        }
        catch (const std::exception& error)
        {
            state.status = error.what();
            LogMaterialError(engine, error);
        }
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!hasOverride);
    if (ImGui::Button("Clear Override"))
    {
        try
        {
            session.EditDocument().ClearModelMaterial(
                selected->entity, static_cast<std::size_t>(selectedSlot));
            state.entity = 0;
            state.dirty = false;
            state.status = "Restored the imported model material for this slot.";
        }
        catch (const std::exception& error)
        {
            state.status = error.what();
            LogMaterialError(engine, error);
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();

    if (state.dirty)
        ImGui::TextDisabled("Unsaved .pgmaterial changes — save the material before saving a scene that references it.");
    if (!state.status.empty())
        ImGui::TextWrapped("%s", state.status.c_str());
    ImGui::TextDisabled("Colour maps: sRGB. Normal / metallic / roughness / AO maps: linear.");
    ImGui::End();
}

} // namespace PlagC::Editor
