#include "pgcpch.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <PlagC/Assets/AssetManager.h>
#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace PlagC::Editor {
namespace {

constexpr const char* CookedModelPayload = "PLAGC_COOKED_MODEL";

std::string CookedAssetKey(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');

    constexpr std::string_view mountedPrefix = "Assets://Cooked/";
    constexpr std::string_view filesystemPrefix = "Assets/Cooked/";
    constexpr std::string_view cookedPrefix = "Cooked/";

    if (path.starts_with(mountedPrefix))
        return path.substr(mountedPrefix.size());
    if (path.starts_with(filesystemPrefix))
        return path.substr(filesystemPrefix.size());
    if (path.starts_with(cookedPrefix))
        return path.substr(cookedPrefix.size());

    const auto found = path.find("/Assets/Cooked/");
    if (found != std::string::npos)
        return path.substr(found + std::string_view("/Assets/Cooked/").size());

    throw std::invalid_argument(
        "Only cooked assets under Assets://Cooked (Assets/Cooked on disk) can be assigned as runtime models");
}

AssetHandle<ModelAsset> LoadModel(Engine& engine, const char* payloadPath)
{
    if (!payloadPath || !*payloadPath)
        throw std::invalid_argument("Cooked model payload path is empty");
    return engine.Assets().Load<ModelAsset>(CookedAssetKey(payloadPath));
}

void LogAssetError(Engine& engine, const std::exception& error)
{
    engine.Logger().Write(Severity::Error, "EDITOR-ASSETS", error.what());
}

const AuthoringEntity* SelectedEntity(const std::vector<AuthoringEntity>& entities)
{
    const auto it = std::find_if(entities.begin(), entities.end(),
        [](const AuthoringEntity& entity) { return entity.selected; });
    return it == entities.end() ? nullptr : &*it;
}

} // namespace

void EditorLayer::CreateCookedModelEntity(Engine& engine, EditorSession& session,
                                          const char* assetPath, Vec3 position)
{
    const auto model = LoadModel(engine, assetPath);
    EntityTemplate entity;
    entity.name = std::filesystem::path(assetPath).stem().string();
    entity.transform.position = position;
    entity.model = ModelComponent{model, true};

    auto& document = session.EditDocument();
    const Entity created = document.Create(std::move(entity));
    document.Select(created);
}

void EditorLayer::AppendAssetInteractions(Engine& engine, EditorSession& session)
{
    if (!session.EditingEnabled())
        return;

    if (showContentBrowser_)
    {
        if (ImGui::Begin("Content Browser"))
        {
            ImGui::SeparatorText("Cooked runtime assets");
            ImGui::TextDisabled(
                "Drag a cooked ModelAsset onto Inspector to assign it, drag it into Viewport to place it, or double-click to create it at the origin.");

            const auto content = dataPanels_.ContentSnapshot();
            if (content)
            {
                for (const auto& entry : *content)
                {
                    if (entry.kind != EditorAssetKind::Cooked)
                        continue;

                    const std::filesystem::path path(entry.path);
                    const std::string label = path.filename().string() + "##CookedDrag" + entry.path;
                    ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", entry.path.c_str());

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        try
                        {
                            CreateCookedModelEntity(engine, session, entry.path.c_str(), Vec3{});
                        }
                        catch (const std::exception& error)
                        {
                            LogAssetError(engine, error);
                        }
                    }

                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
                    {
                        ImGui::SetDragDropPayload(CookedModelPayload,
                                                  entry.path.c_str(), entry.path.size() + 1u);
                        ImGui::Text("Assign model: %s", path.filename().string().c_str());
                        ImGui::EndDragDropSource();
                    }
                }
            }
        }
        ImGui::End();
    }

    const auto snapshot = session.ViewDocument().Snapshot();
    const AuthoringEntity* selected = SelectedEntity(snapshot);

    if (showInspector_)
    {
        if (ImGui::Begin("Inspector"))
        {
            ImGui::SeparatorText("Model Asset");
            if (selected)
            {
                ImGui::TextDisabled("Drop a cooked ModelAsset here to assign or replace the selected entity model.");
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CookedModelPayload))
                    {
                        try
                        {
                            const auto model = LoadModel(engine, static_cast<const char*>(payload->Data));
                            session.EditDocument().SetModel(selected->entity, ModelComponent{model, true});
                        }
                        catch (const std::exception& error)
                        {
                            LogAssetError(engine, error);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
            }
            else
            {
                ImGui::TextDisabled("Select an entity before assigning a model.");
            }
        }
        ImGui::End();
    }
}

} // namespace PlagC::Editor
