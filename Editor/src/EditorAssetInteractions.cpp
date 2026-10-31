#include "pgcpch.h"
#include "EditorDragDrop.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <PlagC/Assets/AssetManager.h>
#include <imgui.h>
#include <algorithm>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>

namespace PlagC::Editor {
namespace {

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
    auto model = engine.Assets().Load<ModelAsset>(CookedAssetKey(payloadPath));
    if (!model)
        throw std::runtime_error("Cooked asset is not a loadable ModelAsset");
    return model;
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

const char* PrimitiveAssetPath(Primitive primitive)
{
    switch (primitive)
    {
    case Primitive::Cube: return "Engine://Primitives/Cube.plasset";
    case Primitive::Plane: return "Engine://Primitives/Plane.plasset";
    case Primitive::Sphere: return "Engine://Primitives/Sphere.plasset";
    case Primitive::Cylinder: return "Engine://Primitives/Cylinder.plasset";
    }
    throw std::invalid_argument("Unknown editor primitive");
}

ModelComponent PrimitiveModel(Engine& engine, Primitive primitive)
{
    auto model = engine.Assets().Load<ModelAsset>(PrimitiveAssetPath(primitive));
    if (!model)
        throw std::runtime_error("Built-in primitive ModelAsset could not be resolved");
    return ModelComponent{std::move(model), true, true};
}

Vec3 CreationAnchor(EditorSession& session)
{
    auto& document = session.EditDocument();
    if (const Entity selected = document.Selection())
    {
        Vec3 anchor = document.Scene().ReadTransform(selected).position;
        anchor.x += 1.25f;
        return anchor;
    }
    return {};
}

void CreateEmpty(EditorSession& session)
{
    EntityTemplate entity;
    entity.name = "Empty";
    entity.transform.position = CreationAnchor(session);
    session.EditDocument().Create(std::move(entity));
}

void CreateCamera(EditorSession& session)
{
    const Vec3 anchor = CreationAnchor(session);
    EntityTemplate entity;
    entity.name = "Camera";
    entity.transform.position = {anchor.x, anchor.y + 2.0f, anchor.z + 5.0f};
    CameraComponent camera;
    camera.target = {anchor.x, anchor.y + 1.0f, anchor.z};
    entity.camera = camera;
    session.EditDocument().Create(std::move(entity));
}

void CreateLight(EditorSession& session, LightType type, const char* name)
{
    const Vec3 anchor = CreationAnchor(session);
    EntityTemplate entity;
    entity.name = name;
    entity.transform.position = type == LightType::Directional
        ? anchor
        : Vec3{anchor.x, anchor.y + 3.0f, anchor.z};

    LightDesc light;
    light.type = type;
    light.direction = {-0.6f, -1.0f, -0.4f};
    if (type == LightType::Directional)
        light.power = 3.141593f;
    else if (type == LightType::Point)
    {
        light.power = 8.0f;
        light.radius = 12.0f;
    }
    else
    {
        light.power = 10.0f;
        light.radius = 15.0f;
        light.innerAngle = 25.0f;
        light.outerAngle = 40.0f;
    }
    entity.light = LightComponent{light};
    session.EditDocument().Create(std::move(entity));
}

void CreatePrimitiveEntity(Engine& engine, EditorSession& session,
                           Primitive primitive, const char* name)
{
    EntityTemplate entity;
    entity.name = name;
    entity.transform.position = CreationAnchor(session);
    if (primitive != Primitive::Plane)
        entity.transform.position.y += 0.5f;
    entity.model = PrimitiveModel(engine, primitive);
    session.EditDocument().Create(std::move(entity));
}

PhysicsComponent PhysicsBody(MotionType motion, ColliderType collider)
{
    PhysicsComponent physics;
    physics.body.motion = motion;
    physics.body.layer = motion == MotionType::Static
        ? CollisionLayer::StaticWorld
        : CollisionLayer::Dynamic;
    physics.collider.type = collider;
    physics.collider.dimensions = {0.5f, 0.5f, 0.5f};
    return physics;
}

void CreatePhysicsEntity(Engine& engine, EditorSession& session, MotionType motion,
                         ColliderType collider, Primitive primitive, const char* name)
{
    EntityTemplate entity;
    entity.name = name;
    entity.transform.position = CreationAnchor(session);
    entity.transform.position.y += motion == MotionType::Static ? 0.5f : 1.5f;
    entity.model = PrimitiveModel(engine, primitive);
    entity.physics = PhysicsBody(motion, collider);
    session.EditDocument().Create(std::move(entity));
}

void DrawModelCreateMenu(Engine& engine, EditorSession& session,
                         const std::shared_ptr<const std::vector<AssetBrowserEntry>>& content)
{
    if (!ImGui::BeginMenu("Model"))
        return;

    bool found = false;
    if (content)
    {
        for (const auto& entry : *content)
        {
            if (entry.kind != EditorAssetKind::Cooked)
                continue;
            const std::filesystem::path path(entry.path);
            if (path.extension() != ".plasset")
                continue;

            found = true;
            const std::string label = path.filename().string() + "##CreateModel" + entry.path;
            if (ImGui::MenuItem(label.c_str()))
            {
                try
                {
                    const Vec3 position = CreationAnchor(session);
                    const auto model = LoadModel(engine, entry.path.c_str());
                    EntityTemplate entity;
                    entity.name = path.stem().string();
                    entity.transform.position = position;
                    entity.model = ModelComponent{model, true, true};
                    session.EditDocument().Create(std::move(entity));
                }
                catch (const std::exception& error)
                {
                    LogAssetError(engine, error);
                }
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", entry.path.c_str());
        }
    }

    if (!found)
        ImGui::TextDisabled(content ? "No cooked assets found." : "Scanning cooked assets...");
    ImGui::EndMenu();
}

void DrawCreateEntityPopup(Engine& engine, EditorSession& session,
                           const std::shared_ptr<const std::vector<AssetBrowserEntry>>& content)
{
    if (!ImGui::BeginPopup("PlagC.CreateEntity"))
        return;

    try
    {
        if (ImGui::MenuItem("Empty"))
            CreateEmpty(session);
        DrawModelCreateMenu(engine, session, content);
        if (ImGui::MenuItem("Camera"))
            CreateCamera(session);

        if (ImGui::BeginMenu("Light"))
        {
            if (ImGui::MenuItem("Directional Light"))
                CreateLight(session, LightType::Directional, "Directional Light");
            if (ImGui::MenuItem("Point Light"))
                CreateLight(session, LightType::Point, "Point Light");
            if (ImGui::MenuItem("Spot Light"))
                CreateLight(session, LightType::Spot, "Spot Light");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Primitive"))
        {
            if (ImGui::MenuItem("Cube"))
                CreatePrimitiveEntity(engine, session, Primitive::Cube, "Cube");
            if (ImGui::MenuItem("Plane"))
                CreatePrimitiveEntity(engine, session, Primitive::Plane, "Plane");
            if (ImGui::MenuItem("Sphere"))
                CreatePrimitiveEntity(engine, session, Primitive::Sphere, "Sphere");
            if (ImGui::MenuItem("Cylinder"))
                CreatePrimitiveEntity(engine, session, Primitive::Cylinder, "Cylinder");
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Physics"))
        {
            if (ImGui::MenuItem("Static Box"))
                CreatePhysicsEntity(engine, session, MotionType::Static, ColliderType::Box,
                                    Primitive::Cube, "Static Box");
            if (ImGui::MenuItem("Dynamic Box"))
                CreatePhysicsEntity(engine, session, MotionType::Dynamic, ColliderType::Box,
                                    Primitive::Cube, "Dynamic Box");
            if (ImGui::MenuItem("Kinematic Box"))
                CreatePhysicsEntity(engine, session, MotionType::Kinematic, ColliderType::Box,
                                    Primitive::Cube, "Kinematic Box");
            if (ImGui::MenuItem("Dynamic Sphere"))
                CreatePhysicsEntity(engine, session, MotionType::Dynamic, ColliderType::Sphere,
                                    Primitive::Sphere, "Dynamic Sphere");
            if (ImGui::MenuItem("Dynamic Cylinder"))
                CreatePhysicsEntity(engine, session, MotionType::Dynamic, ColliderType::Cylinder,
                                    Primitive::Cylinder, "Dynamic Cylinder");
            ImGui::EndMenu();
        }
    }
    catch (const std::exception& error)
    {
        LogAssetError(engine, error);
    }

    ImGui::EndPopup();
}

struct AssetBounds
{
    Vec3 minimum{};
    Vec3 maximum{};
    bool valid = false;
};

AssetBounds CalculateBounds(const ModelAsset& asset)
{
    AssetBounds result;
    result.minimum = {std::numeric_limits<float>::max(), std::numeric_limits<float>::max(),
                      std::numeric_limits<float>::max()};
    result.maximum = {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest(),
                      std::numeric_limits<float>::lowest()};
    for (const auto& mesh : asset.meshes)
    {
        for (const auto& vertex : mesh.geometry.vertices)
        {
            const Vec3 p = vertex.position;
            result.minimum.x = std::min(result.minimum.x, p.x);
            result.minimum.y = std::min(result.minimum.y, p.y);
            result.minimum.z = std::min(result.minimum.z, p.z);
            result.maximum.x = std::max(result.maximum.x, p.x);
            result.maximum.y = std::max(result.maximum.y, p.y);
            result.maximum.z = std::max(result.maximum.z, p.z);
            result.valid = true;
        }
    }
    return result;
}

ModelComponent ReplacementModel(const AuthoringEntity& selected, AssetHandle<ModelAsset> asset)
{
    ModelComponent result{std::move(asset), true, true};
    if (selected.record.model)
    {
        result.visible = selected.record.model->visible;
        result.castShadows = selected.record.model->castShadows;
    }
    return result;
}

bool DrawModelPicker(Engine& engine, EditorSession& session, const AuthoringEntity& selected,
                     const std::shared_ptr<const std::vector<AssetBrowserEntry>>& content)
{
    if (!ImGui::BeginPopup("PlagC.ModelPicker"))
        return false;

    bool changed = false;
    bool found = false;
    if (content)
    {
        for (const auto& entry : *content)
        {
            if (entry.kind != EditorAssetKind::Cooked ||
                std::filesystem::path(entry.path).extension() != ".plasset")
                continue;
            found = true;
            const std::filesystem::path path(entry.path);
            const std::string label = path.filename().string() + "##ReplaceModel" + entry.path;
            if (ImGui::Selectable(label.c_str()))
            {
                auto asset = LoadModel(engine, entry.path.c_str());
                session.EditDocument().SetModel(selected.entity,
                    ReplacementModel(selected, std::move(asset)));
                changed = true;
                ImGui::CloseCurrentPopup();
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", entry.path.c_str());
        }
    }
    if (!found)
        ImGui::TextDisabled(content ? "No cooked ModelAsset found." : "Scanning cooked assets...");
    ImGui::EndPopup();
    return changed;
}

void DrawModelAssetDetails(const ModelAsset& asset)
{
    const AssetBounds bounds = CalculateBounds(asset);
    std::size_t vertices = 0;
    std::size_t indices = 0;
    for (const auto& mesh : asset.meshes)
    {
        vertices += mesh.geometry.vertices.size();
        indices += mesh.geometry.indices.size();
    }

    if (ImGui::CollapsingHeader("Mesh / Submesh Information", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Meshes: %zu   Vertices: %zu   Indices: %zu", asset.meshes.size(), vertices, indices);
        if (ImGui::BeginTable("ModelMeshes", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_Resizable))
        {
            ImGui::TableSetupColumn("Submesh");
            ImGui::TableSetupColumn("Vertices");
            ImGui::TableSetupColumn("Indices");
            ImGui::TableSetupColumn("Material Slot");
            ImGui::TableHeadersRow();
            for (std::size_t i = 0; i < asset.meshes.size(); ++i)
            {
                const auto& mesh = asset.meshes[i];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%zu: %s", i, mesh.name.empty() ? "<unnamed>" : mesh.name.c_str());
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%zu", mesh.geometry.vertices.size());
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%zu", mesh.geometry.indices.size());
                ImGui::TableSetColumnIndex(3);
                if (mesh.material < asset.materials.size())
                {
                    const auto& material = asset.materials[mesh.material];
                    ImGui::Text("%u: %s", mesh.material,
                        material.name.empty() ? "<unnamed>" : material.name.c_str());
                }
                else
                    ImGui::TextDisabled("Invalid slot %u", mesh.material);
            }
            ImGui::EndTable();
        }
    }

    if (ImGui::CollapsingHeader("Material Slots", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::TextDisabled("Imported slots are read-only here; material authoring is handled by the Material Inspector.");
        for (std::size_t i = 0; i < asset.materials.size(); ++i)
        {
            const auto& material = asset.materials[i];
            ImGui::BulletText("Slot %zu  %s", i, material.name.empty() ? "<unnamed>" : material.name.c_str());
        }
    }

    if (ImGui::CollapsingHeader("Bounds", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!bounds.valid)
            ImGui::TextDisabled("No vertex bounds available.");
        else
        {
            const Vec3 size{bounds.maximum.x - bounds.minimum.x,
                            bounds.maximum.y - bounds.minimum.y,
                            bounds.maximum.z - bounds.minimum.z};
            ImGui::Text("Min:  %.3f, %.3f, %.3f", bounds.minimum.x, bounds.minimum.y, bounds.minimum.z);
            ImGui::Text("Max:  %.3f, %.3f, %.3f", bounds.maximum.x, bounds.maximum.y, bounds.maximum.z);
            ImGui::Text("Size: %.3f, %.3f, %.3f", size.x, size.y, size.z);
        }
    }

    if (!asset.warnings.empty() && ImGui::CollapsingHeader("Import Warnings"))
        for (const auto& warning : asset.warnings)
            ImGui::BulletText("%s", warning.c_str());
}

void DrawModelInspector(Engine& engine, EditorSession& session, EditorDataPanels& panels,
                        const AuthoringEntity* selected,
                        const std::shared_ptr<const std::vector<AssetBrowserEntry>>& content)
{
    ImGui::SeparatorText("Model / Mesh Inspector");
    if (!selected)
    {
        ImGui::TextDisabled("Select an entity to inspect its model asset.");
        return;
    }

    auto& document = session.EditDocument();
    if (!selected->record.model || !selected->record.model->asset)
    {
        ImGui::TextDisabled("No ModelAsset assigned.");
        if (ImGui::Button("Browse Model..."))
            ImGui::OpenPopup("PlagC.ModelPicker");
        try { (void)DrawModelPicker(engine, session, *selected, content); }
        catch (const std::exception& error) { LogAssetError(engine, error); }
        ImGui::TextDisabled("You can also drag a cooked ModelAsset from Content Browser onto this Inspector.");
        return;
    }

    const ModelComponent model = *selected->record.model;
    const auto asset = model.asset;
    ImGui::TextDisabled("Model Asset");
    ImGui::TextWrapped("%s", asset->metadata.path.c_str());
    ImGui::Text("Generation: %llu   Cooked bytes: %llu",
        static_cast<unsigned long long>(asset->metadata.generation),
        static_cast<unsigned long long>(asset->metadata.bytes));

    if (ImGui::Button("Browse / Replace..."))
        ImGui::OpenPopup("PlagC.ModelPicker");
    try { (void)DrawModelPicker(engine, session, *selected, content); }
    catch (const std::exception& error) { LogAssetError(engine, error); }
    ImGui::SameLine();
    if (ImGui::Button("Locate in Browser"))
        panels.RevealAsset(asset->metadata.path);
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
    {
        try
        {
            const std::string path = asset->metadata.path;
            engine.Assets().Reload(path);
            engine.Assets().Wait();
            auto reloaded = engine.Assets().Get<ModelAsset>(path);
            if (!reloaded)
                throw std::runtime_error("Reload completed without a ModelAsset result");
            document.ReloadModelAsset(selected->entity, std::move(reloaded));
            panels.RefreshAssets(engine);
            engine.Logger().Write(Severity::Info, "EDITOR-ASSETS", "Reloaded model asset: " + path);
        }
        catch (const std::exception& error) { LogAssetError(engine, error); }
    }

    bool visible = model.visible;
    if (ImGui::Checkbox("Visible##ModelMeshInspector", &visible))
    {
        try { document.SetModelVisible(selected->entity, visible); }
        catch (const std::exception& error) { LogAssetError(engine, error); }
    }
    bool castShadows = model.castShadows;
    if (ImGui::Checkbox("Cast Shadows", &castShadows))
    {
        try { document.SetModelCastShadows(selected->entity, castShadows); }
        catch (const std::exception& error) { LogAssetError(engine, error); }
    }

    DrawModelAssetDetails(*asset);

    if (ImGui::Button("Remove Model##ModelMeshInspector"))
    {
        try { document.RemoveModel(selected->entity); }
        catch (const std::exception& error) { LogAssetError(engine, error); }
    }
}

} // namespace

void EditorLayer::CreateCookedModelEntity(Engine& engine, EditorSession& session,
                                          const char* assetPath, Vec3 position)
{
    const auto model = LoadModel(engine, assetPath);
    EntityTemplate entity;
    entity.name = std::filesystem::path(assetPath).stem().string();
    entity.transform.position = position;
    entity.model = ModelComponent{model, true, true};

    session.EditDocument().Create(std::move(entity));
}

void EditorLayer::AppendAssetInteractions(Engine& engine, EditorSession& session)
{
    const std::string activation = dataPanels_.ConsumeCookedModelActivation();
    if (!session.EditingEnabled())
        return;

    if (!activation.empty())
    {
        try
        {
            CreateCookedModelEntity(engine, session, activation.c_str(), Vec3{});
        }
        catch (const std::exception& error)
        {
            LogAssetError(engine, error);
        }
    }

    const auto content = dataPanels_.ContentSnapshot();

    // Re-open Hierarchy in the late interaction pass so creation uses the same docked panel
    // without coupling the core hierarchy renderer to model/renderer resource concerns.
    if (showHierarchy_)
    {
        if (ImGui::Begin("Hierarchy"))
        {
            ImGui::SeparatorText("Create Entity");
            if (ImGui::Button("+ Create...", ImVec2(-1.0f, 0.0f)))
                ImGui::OpenPopup("PlagC.CreateEntity");
            DrawCreateEntityPopup(engine, session, content);
        }
        ImGui::End();
    }

    const auto snapshot = session.ViewDocument().Snapshot();
    const AuthoringEntity* selected = SelectedEntity(snapshot);

    if (showInspector_)
    {
        if (ImGui::Begin("Inspector"))
        {
            DrawModelInspector(engine, session, dataPanels_, selected, content);
            ImGui::SeparatorText("Model Asset Drop Target");
            if (selected)
            {
                ImGui::Button("Drop cooked ModelAsset here##ModelAssetDrop", ImVec2(-1.0f, 0.0f));
                const ImVec2 dropMin = ImGui::GetItemRectMin();
                const ImVec2 dropMax = ImGui::GetItemRectMax();
                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DragDrop::CookedModel))
                    {
                        try
                        {
                            const std::string path = DragDrop::TextPayload(*payload);
                            auto model = LoadModel(engine, path.c_str());
                            session.EditDocument().SetModel(selected->entity,
                                ReplacementModel(*selected, std::move(model)));
                        }
                        catch (const std::exception& error)
                        {
                            LogAssetError(engine, error);
                        }
                    }
                    ImGui::EndDragDropTarget();
                }
                if (const ImGuiPayload* payload = ImGui::GetDragDropPayload())
                {
                    const bool accepted = payload->IsDataType(DragDrop::CookedModel);
                    DragDrop::DrawRectFeedback(dropMin, dropMax, accepted,
                                               "Assign cooked model to selected entity",
                                               "Model Inspector accepts cooked ModelAsset assets");
                }
            }
            else
            {
                ImGui::TextDisabled("Select an entity before assigning a model.");
                if (ImGui::GetDragDropPayload())
                    DragDrop::DrawInlineFeedback(false, "",
                                                 "Select an entity before assigning a model");
            }
        }
        ImGui::End();
    }
}

} // namespace PlagC::Editor
