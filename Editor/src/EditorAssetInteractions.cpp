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
    const std::string marker = "Assets/Cooked/";
    if (path.starts_with(marker))
        return path.substr(marker.size());
    const auto found = path.find("/" + marker);
    if (found != std::string::npos)
        return path.substr(found + marker.size() + 1u);
    throw std::invalid_argument("Only files under Assets/Cooked can be assigned as runtime models");
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
                "Drag a cooked ModelAsset onto Inspector to assign it, or double-click to create an entity at the origin.");
            for (const auto& entry : dataPanels_.ContentSnapshot())
            {
                if (entry.kind != EditorAssetKind::Cooked || entry.directory)
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
                        const auto model = LoadModel(engine, entry.path.c_str());
                        EntityTemplate entity;
                        entity.name = path.stem().string();
                        entity.model = ModelComponent{model, true};
                        auto& document = session.EditDocument();
                        const Entity created = document.Create(std::move(entity));
                        document.Select(created);
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
