#include "pgcpch.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <PlagC/Assets/AssetManager.h>
#include <imgui.h>
#include <algorithm>
#include <array>
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

struct PrimitiveResources
{
    Renderer* owner = nullptr;
    MaterialHandle material{};
    std::array<MeshHandle, 4> meshes{};
};

PrimitiveResources& EditorPrimitives()
{
    // These resources intentionally live for the renderer lifetime. The editor process owns a
    // single Renderer, which releases all remaining resource handles during renderer shutdown.
    static PrimitiveResources resources;
    return resources;
}

std::size_t PrimitiveIndex(Primitive primitive)
{
    switch (primitive)
    {
    case Primitive::Cube: return 0;
    case Primitive::Plane: return 1;
    case Primitive::Sphere: return 2;
    case Primitive::Cylinder: return 3;
    }
    throw std::invalid_argument("Unknown editor primitive");
}

RenderableComponent PrimitiveRenderable(Engine& engine, Primitive primitive)
{
    auto& renderer = engine.Graphics();
    auto& resources = EditorPrimitives();
    if (!resources.owner)
        resources.owner = &renderer;
    if (resources.owner != &renderer)
        throw std::logic_error("Editor primitive cache belongs to a different renderer");

    if (!resources.material)
    {
        MaterialDesc material;
        material.colour = {0.72f, 0.74f, 0.78f};
        material.roughness = 0.55f;
        resources.material = renderer.CreateMaterial(material);
    }

    auto& mesh = resources.meshes[PrimitiveIndex(primitive)];
    if (!mesh)
        mesh = renderer.CreateMesh(MakePrimitive(primitive));

    return RenderableComponent{mesh, resources.material, true, true, 10};
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
    entity.renderable = PrimitiveRenderable(engine, primitive);
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
    entity.renderable = PrimitiveRenderable(engine, primitive);
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
                    entity.model = ModelComponent{model, true};
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

} // namespace

void EditorLayer::CreateCookedModelEntity(Engine& engine, EditorSession& session,
                                          const char* assetPath, Vec3 position)
{
    const auto model = LoadModel(engine, assetPath);
    EntityTemplate entity;
    entity.name = std::filesystem::path(assetPath).stem().string();
    entity.transform.position = position;
    entity.model = ModelComponent{model, true};

    session.EditDocument().Create(std::move(entity));
}

void EditorLayer::AppendAssetInteractions(Engine& engine, EditorSession& session)
{
    if (!session.EditingEnabled())
        return;

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

    if (showContentBrowser_)
    {
        if (ImGui::Begin("Content Browser"))
        {
            ImGui::SeparatorText("Cooked runtime assets");
            ImGui::TextDisabled(
                "Drag a cooked ModelAsset onto Inspector to assign it, drag it into Viewport to place it, or double-click to create it at the origin.");

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
