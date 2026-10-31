#include "pgcpch.h"
#include "EditorDragDrop.h"
#include "EditorSceneHierarchy.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cstdio>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace PlagC::Editor {
namespace {

enum class PendingKind {
    None,
    Duplicate,
    Delete,
    Reparent,
    Visibility
};

struct PendingAction {
    PendingKind kind = PendingKind::None;
    std::uint64_t subject = 0;
    std::uint64_t target = 0;
    bool flag = false;
};

void LogEditorError(Engine& engine, const std::exception& error)
{
    engine.Logger().Write(Severity::Error, "EDITOR-HIERARCHY", error.what());
}

std::string Lower(std::string_view value)
{
    std::string result;
    result.reserve(value.size());
    for (const unsigned char c : value)
        result.push_back(static_cast<char>(std::tolower(c)));
    return result;
}

const char* TypeName(const AuthoringEntity& item)
{
    if (item.record.camera)
        return item.activeCamera ? "Active Camera" : "Camera";
    if (item.record.light)
        return "Light";
    if (item.record.model)
        return "Model";
    if (item.record.renderable)
        return "Mesh";
    if (item.record.physics)
        return "Physics";
    if (item.record.animator)
        return "Animator";
    return "Entity";
}

const char* TypeBadge(const AuthoringEntity& item)
{
    if (item.record.camera)
        return item.activeCamera ? "CAM*" : "CAM";
    if (item.record.light)
        return "LGT";
    if (item.record.model)
        return "MDL";
    if (item.record.renderable)
        return "MSH";
    if (item.record.physics)
        return "PHY";
    if (item.record.animator)
        return "ANM";
    return "ENT";
}

bool HasVisual(const AuthoringEntity& item)
{
    return item.record.model.has_value() || item.record.renderable.has_value();
}

bool VisualVisible(const AuthoringEntity& item)
{
    if (item.record.model)
        return item.record.model->visible;
    if (item.record.renderable)
        return item.record.renderable->visible;
    return true;
}

bool MatchesFilter(const AuthoringEntity& item, std::string_view filter)
{
    if (filter.empty())
        return true;
    const std::string needle = Lower(filter);
    return Lower(item.record.name).find(needle) != std::string::npos ||
           Lower(TypeName(item)).find(needle) != std::string::npos;
}

void ShowTypeTooltip(const AuthoringEntity& item)
{
    if (!ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        return;
    ImGui::BeginTooltip();
    ImGui::TextUnformatted(TypeName(item));
    if (item.record.model) ImGui::TextDisabled("ModelComponent");
    if (item.record.renderable) ImGui::TextDisabled("RenderableComponent");
    if (item.record.camera) ImGui::TextDisabled("CameraComponent");
    if (item.record.light) ImGui::TextDisabled("LightComponent");
    if (item.record.physics) ImGui::TextDisabled("PhysicsComponent");
    if (item.record.animator) ImGui::TextDisabled("AnimatorComponent");
    ImGui::EndTooltip();
}

void Tooltip(const char* text)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", text);
}

bool WouldCreateCycle(const std::unordered_map<std::uint64_t, const AuthoringEntity*>& byID,
                      std::uint64_t dragged,
                      std::uint64_t target)
{
    if (!dragged || !target)
        return false;

    std::uint64_t current = target;
    for (std::size_t guard = 0; current && guard <= byID.size(); ++guard)
    {
        if (current == dragged)
            return true;
        const auto found = byID.find(current);
        if (found == byID.end())
            break;
        current = found->second->parentPersistentID;
    }
    return false;
}

} // namespace

bool EditorSceneHierarchy::Locked(std::uint64_t persistentID) const noexcept
{
    return persistentID != 0 && locked_.contains(persistentID);
}

void EditorSceneHierarchy::Reset() noexcept
{
    documentIdentity_ = nullptr;
    search_.fill('\0');
    rename_.fill('\0');
    expanded_.clear();
    locked_.clear();
    renamingID_ = 0;
    renameFocusPending_ = false;
    expansionInitialized_ = false;
}

void EditorSceneHierarchy::ResetForDocument(const SceneDocument& document)
{
    if (documentIdentity_ == &document)
        return;
    Reset();
    documentIdentity_ = &document;
}

void EditorSceneHierarchy::BeginRename(const AuthoringEntity& item)
{
    renamingID_ = item.persistentID;
    std::snprintf(rename_.data(), rename_.size(), "%s", item.record.name.c_str());
    renameFocusPending_ = true;
}

void EditorSceneHierarchy::Draw(Engine& engine, EditorSession& session, bool& open)
{
    ResetForDocument(session.Document());

    if (!ImGui::Begin("Hierarchy", &open))
    {
        ImGui::End();
        return;
    }

    const bool editing = session.EditingEnabled();
    const auto entities = session.ViewDocument().Snapshot();

    std::unordered_map<std::uint64_t, const AuthoringEntity*> byID;
    std::unordered_map<std::uint64_t, std::vector<const AuthoringEntity*>> children;
    byID.reserve(entities.size());
    children.reserve(entities.size() + 1);
    for (const auto& item : entities)
    {
        byID.emplace(item.persistentID, &item);
        children[item.parentPersistentID].push_back(&item);
    }

    auto prune = [&](std::unordered_set<std::uint64_t>& state)
    {
        for (auto it = state.begin(); it != state.end();)
            if (byID.contains(*it))
                ++it;
            else
                it = state.erase(it);
    };
    prune(expanded_);
    prune(locked_);
    if (renamingID_ && !byID.contains(renamingID_))
    {
        renamingID_ = 0;
        renameFocusPending_ = false;
    }

    if (!expansionInitialized_)
    {
        for (const auto& item : entities)
            if (!item.parentPersistentID && children.contains(item.persistentID))
                expanded_.insert(item.persistentID);
        expansionInitialized_ = true;
    }

    ImGui::BeginDisabled(!editing);
    if (ImGui::Button("+ Empty"))
    {
        try { session.EditDocument().Create(); }
        catch (const std::exception& error) { LogEditorError(engine, error); }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("%zu %s", entities.size(), entities.size() == 1 ? "entity" : "entities");

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##HierarchySearch", "Search name or type...",
                             search_.data(), search_.size());

    if (ImGui::SmallButton("Expand All"))
    {
        for (const auto& [id, list] : children)
            if (id && !list.empty())
                expanded_.insert(id);
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Collapse All"))
        expanded_.clear();
    ImGui::SameLine();
    ImGui::BeginDisabled(search_[0] == '\0');
    if (ImGui::SmallButton("Clear Search"))
        search_.fill('\0');
    ImGui::EndDisabled();

    PendingAction pending{};

    if (editing)
    {
        ImGui::Separator();
        ImGui::Selectable("Scene Root  (drop here)", false, ImGuiSelectableFlags_AllowDoubleClick,
                          ImVec2(0.0f, ImGui::GetFrameHeight()));
        const ImVec2 rootDropMin = ImGui::GetItemRectMin();
        const ImVec2 rootDropMax = ImGui::GetItemRectMax();
        const bool rootDropHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
        Tooltip("Drop an entity here to remove its parent");
        if (rootDropHovered && ImGui::GetDragDropPayload())
        {
            bool accepted = false;
            const ImGuiPayload* active = ImGui::GetDragDropPayload();
            if (active->IsDataType(DragDrop::Entity))
            {
                try
                {
                    const std::uint64_t dragged = DragDrop::EntityIDPayload(*active);
                    accepted = !Locked(dragged) && byID.contains(dragged);
                }
                catch (...) {}
            }
            DragDrop::DrawRectFeedback(rootDropMin, rootDropMax, accepted,
                                       "Move entity to Scene Root",
                                       "Scene Root accepts an unlocked hierarchy entity");
        }
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DragDrop::Entity))
            {
                try
                {
                    const std::uint64_t dragged = DragDrop::EntityIDPayload(*payload);
                    if (!Locked(dragged) && byID.contains(dragged))
                        pending = {PendingKind::Reparent, dragged, 0, false};
                }
                catch (const std::exception& error) { LogEditorError(engine, error); }
            }
            ImGui::EndDragDropTarget();
        }
    }

    const std::string filter = search_.data();
    const bool filtering = !filter.empty();
    std::unordered_set<std::uint64_t> filteredVisible;
    if (filtering)
    {
        for (const auto& item : entities)
        {
            if (!MatchesFilter(item, filter))
                continue;
            filteredVisible.insert(item.persistentID);
            std::uint64_t parent = item.parentPersistentID;
            while (parent)
            {
                if (!filteredVisible.insert(parent).second)
                    break;
                const auto found = byID.find(parent);
                parent = found == byID.end() ? 0 : found->second->parentPersistentID;
            }
        }
    }

    const auto rowVisible = [&](std::uint64_t id)
    {
        return !filtering || filteredVisible.contains(id);
    };

    const Entity selectedEntity = editing ? session.EditDocument().Selection() : Entity{};
    const std::uint64_t selectedID =
        selectedEntity ? session.EditDocument().PersistentID(selectedEntity) : 0;
    if (editing && selectedID && !Locked(selectedID) &&
        ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
        !ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F2, false))
    {
        const auto found = byID.find(selectedID);
        if (found != byID.end())
            BeginRename(*found->second);
    }

    bool drewRow = false;
    if (ImGui::BeginTable("##HierarchyRows", 3,
                          ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_NoBordersInBody))
    {
        ImGui::TableSetupColumn("Entity", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Visible", ImGuiTableColumnFlags_WidthFixed, 34.0f);
        ImGui::TableSetupColumn("Lock", ImGuiTableColumnFlags_WidthFixed, 34.0f);

        std::function<void(std::uint64_t, int)> drawChildren;
        drawChildren = [&](std::uint64_t parentID, int depth)
        {
            const auto found = children.find(parentID);
            if (found == children.end())
                return;

            for (const AuthoringEntity* item : found->second)
            {
                if (!rowVisible(item->persistentID))
                    continue;

                bool hasVisibleChildren = false;
                if (const auto childIt = children.find(item->persistentID); childIt != children.end())
                    hasVisibleChildren = std::any_of(childIt->second.begin(), childIt->second.end(),
                        [&](const AuthoringEntity* child) { return rowVisible(child->persistentID); });

                bool rowOpen = filtering ? hasVisibleChildren : expanded_.contains(item->persistentID);
                const bool locked = Locked(item->persistentID);

                ImGui::PushID(static_cast<int>(item->persistentID & 0x7fffffff));
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (depth > 0)
                    ImGui::Indent(static_cast<float>(depth) * 14.0f);

                const float arrowWidth = ImGui::GetFrameHeight();
                if (hasVisibleChildren)
                {
                    if (ImGui::ArrowButton("##expand", rowOpen ? ImGuiDir_Down : ImGuiDir_Right) &&
                        !filtering)
                    {
                        if (rowOpen)
                            expanded_.erase(item->persistentID);
                        else
                            expanded_.insert(item->persistentID);
                        rowOpen = !rowOpen;
                    }
                }
                else
                    ImGui::Dummy(ImVec2(arrowWidth, ImGui::GetFrameHeight()));

                ImGui::SameLine(0.0f, 4.0f);
                ImGui::TextDisabled("[%s]", TypeBadge(*item));
                ShowTypeTooltip(*item);
                ImGui::SameLine(0.0f, 5.0f);

                bool nameItemDrawn = false;
                if (editing && renamingID_ == item->persistentID && !locked)
                {
                    ImGui::SetNextItemWidth(-FLT_MIN);
                    if (renameFocusPending_)
                    {
                        ImGui::SetKeyboardFocusHere();
                        renameFocusPending_ = false;
                    }
                    const bool commit = ImGui::InputText(
                        "##rename", rename_.data(), rename_.size(),
                        ImGuiInputTextFlags_EnterReturnsTrue |
                        ImGuiInputTextFlags_AutoSelectAll);
                    nameItemDrawn = true;
                    const bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false);
                    const bool finish = commit || ImGui::IsItemDeactivatedAfterEdit();
                    if (cancel)
                    {
                        renamingID_ = 0;
                        renameFocusPending_ = false;
                    }
                    else if (finish)
                    {
                        try
                        {
                            const Entity entity =
                                session.EditDocument().FindByPersistentID(item->persistentID);
                            if (entity && rename_[0] != '\0')
                                session.EditDocument().Rename(entity, rename_.data());
                        }
                        catch (const std::exception& error) { LogEditorError(engine, error); }
                        renamingID_ = 0;
                        renameFocusPending_ = false;
                    }
                }
                else
                {
                    if (locked)
                        ImGui::PushStyleColor(ImGuiCol_Text,
                                            ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
                    const bool clicked = ImGui::Selectable(
                        item->record.name.c_str(), item->selected,
                        ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, 0.0f));
                    const ImVec2 rowDropMin = ImGui::GetItemRectMin();
                    const ImVec2 rowDropMax = ImGui::GetItemRectMax();
                    const bool rowDropHovered = ImGui::IsItemHovered(
                        ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                    nameItemDrawn = true;
                    if (locked)
                        ImGui::PopStyleColor();

                    if (clicked && editing && !locked)
                    {
                        try { session.EditDocument().SelectPersistent(item->persistentID); }
                        catch (const std::exception& error) { LogEditorError(engine, error); }
                    }

                    if (editing && !locked && ImGui::BeginDragDropSource())
                    {
                        const std::uint64_t id = item->persistentID;
                        ImGui::SetDragDropPayload(DragDrop::Entity, &id, sizeof(id));
                        ImGui::Text("[%s] %s", TypeBadge(*item), item->record.name.c_str());
                        ImGui::EndDragDropSource();
                    }

                    if (rowDropHovered && ImGui::GetDragDropPayload())
                    {
                        bool accepted = false;
                        const ImGuiPayload* active = ImGui::GetDragDropPayload();
                        if (editing && !locked && active->IsDataType(DragDrop::Entity))
                        {
                            try
                            {
                                const std::uint64_t dragged = DragDrop::EntityIDPayload(*active);
                                accepted = byID.contains(dragged) && !Locked(dragged) &&
                                           dragged != item->persistentID &&
                                           !WouldCreateCycle(byID, dragged, item->persistentID);
                            }
                            catch (...) {}
                        }
                        DragDrop::DrawRectFeedback(
                            rowDropMin, rowDropMax, accepted,
                            "Reparent entity here",
                            locked
                                ? "Target entity is locked"
                                : "Drop must be an unlocked entity and cannot create a hierarchy cycle");
                    }

                    if (editing && !locked && ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload =
                                ImGui::AcceptDragDropPayload(DragDrop::Entity))
                        {
                            try
                            {
                                const std::uint64_t dragged = DragDrop::EntityIDPayload(*payload);
                                if (byID.contains(dragged) && !Locked(dragged) &&
                                    dragged != item->persistentID &&
                                    !WouldCreateCycle(byID, dragged, item->persistentID))
                                {
                                    pending = {PendingKind::Reparent, dragged,
                                               item->persistentID, false};
                                }
                            }
                            catch (const std::exception& error) { LogEditorError(engine, error); }
                        }
                        ImGui::EndDragDropTarget();
                    }
                }

                if (nameItemDrawn && ImGui::BeginPopupContextItem("##EntityContext"))
                {
                    if (ImGui::MenuItem("Rename", "F2", false, editing && !locked))
                        BeginRename(*item);
                    if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, editing && !locked))
                        pending = {PendingKind::Duplicate, item->persistentID, 0, false};
                    if (item->parentPersistentID &&
                        ImGui::MenuItem("Move to Scene Root", nullptr, false, editing && !locked))
                        pending = {PendingKind::Reparent, item->persistentID, 0, false};
                    if (HasVisual(*item))
                    {
                        const bool visible = VisualVisible(*item);
                        if (ImGui::MenuItem(visible ? "Hide" : "Show", nullptr, false,
                                            editing && !locked))
                            pending = {PendingKind::Visibility, item->persistentID, 0, !visible};
                    }
                    if (ImGui::MenuItem(locked ? "Unlock" : "Lock", nullptr, false, editing))
                    {
                        if (locked)
                            locked_.erase(item->persistentID);
                        else
                        {
                            locked_.insert(item->persistentID);
                            try
                            {
                                auto& document = session.EditDocument();
                                if (document.Selection() &&
                                    document.PersistentID(document.Selection()) ==
                                        item->persistentID)
                                    document.Select({});
                            }
                            catch (const std::exception& error)
                            {
                                LogEditorError(engine, error);
                            }
                        }
                    }
                    ImGui::Separator();
                    if (ImGui::MenuItem("Delete", "Delete", false, editing && !locked))
                        pending = {PendingKind::Delete, item->persistentID, 0, false};
                    ImGui::EndPopup();
                }

                if (depth > 0)
                    ImGui::Unindent(static_cast<float>(depth) * 14.0f);

                ImGui::TableSetColumnIndex(1);
                if (HasVisual(*item))
                {
                    const bool visible = VisualVisible(*item);
                    ImGui::BeginDisabled(!editing || locked);
                    if (ImGui::SmallButton(visible ? "V" : "H"))
                        pending = {PendingKind::Visibility, item->persistentID, 0, !visible};
                    ImGui::EndDisabled();
                    Tooltip(visible ? "Visible - click to hide" : "Hidden - click to show");
                }
                else
                    ImGui::TextDisabled("-");

                ImGui::TableSetColumnIndex(2);
                ImGui::BeginDisabled(!editing);
                if (ImGui::SmallButton(locked ? "L" : "U"))
                {
                    if (locked)
                        locked_.erase(item->persistentID);
                    else
                    {
                        locked_.insert(item->persistentID);
                        try
                        {
                            auto& document = session.EditDocument();
                            if (document.Selection() &&
                                document.PersistentID(document.Selection()) ==
                                    item->persistentID)
                                document.Select({});
                        }
                        catch (const std::exception& error) { LogEditorError(engine, error); }
                    }
                }
                ImGui::EndDisabled();
                Tooltip(locked
                            ? "Locked - cannot select, rename, delete, reparent or pick"
                            : "Unlocked - click to lock authoring interactions");

                ImGui::PopID();
                drewRow = true;

                if (hasVisibleChildren && rowOpen)
                    drawChildren(item->persistentID, depth + 1);
            }
        };

        drawChildren(0, 0);
        ImGui::EndTable();
    }

    if (entities.empty())
    {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextDisabled("This scene has no entities yet.");
        ImGui::TextWrapped(
            "Create an empty entity here, or drag a cooked model into the Viewport.");
        ImGui::BeginDisabled(!editing);
        if (ImGui::Button("Create First Entity"))
        {
            try { session.EditDocument().Create(); }
            catch (const std::exception& error) { LogEditorError(engine, error); }
        }
        ImGui::EndDisabled();
    }
    else if (filtering && !drewRow)
    {
        ImGui::Spacing();
        ImGui::TextDisabled("No entities match \"%s\".", filter.c_str());
        if (ImGui::Button("Clear filter"))
            search_.fill('\0');
    }

    if (!editing)
    {
        ImGui::Separator();
        ImGui::TextDisabled("Play mode: hierarchy is read-only.");
    }

    if (ImGui::BeginPopupContextWindow(
            "##HierarchyWindowContext",
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
    {
        if (ImGui::MenuItem("Create Empty", nullptr, false, editing))
        {
            try { session.EditDocument().Create(); }
            catch (const std::exception& error) { LogEditorError(engine, error); }
        }
        if (ImGui::MenuItem("Expand All"))
            for (const auto& [id, list] : children)
                if (id && !list.empty()) expanded_.insert(id);
        if (ImGui::MenuItem("Collapse All"))
            expanded_.clear();
        ImGui::EndPopup();
    }

    if (editing && pending.kind != PendingKind::None)
    {
        try
        {
            auto& document = session.EditDocument();
            const Entity subject = document.FindByPersistentID(pending.subject);
            if (subject)
            {
                switch (pending.kind)
                {
                case PendingKind::Duplicate:
                    if (!Locked(pending.subject))
                        document.Duplicate(subject);
                    break;
                case PendingKind::Delete:
                    if (!Locked(pending.subject))
                    {
                        document.Destroy(subject);
                        expanded_.erase(pending.subject);
                        locked_.erase(pending.subject);
                        if (renamingID_ == pending.subject)
                            renamingID_ = 0;
                    }
                    break;
                case PendingKind::Reparent:
                    if (!Locked(pending.subject))
                    {
                        if (pending.target)
                        {
                            const Entity target =
                                document.FindByPersistentID(pending.target);
                            if (target && !Locked(pending.target))
                                document.SetParent(subject, target);
                        }
                        else
                            document.ClearParent(subject);
                    }
                    break;
                case PendingKind::Visibility:
                    if (!Locked(pending.subject))
                    {
                        const auto record = document.Scene().Read(subject);
                        if (record.model)
                            document.SetModelVisible(subject, pending.flag);
                        else if (record.renderable)
                        {
                            auto renderable = *record.renderable;
                            renderable.visible = pending.flag;
                            document.SetRenderable(subject, renderable);
                        }
                    }
                    break;
                case PendingKind::None:
                    break;
                }
            }
        }
        catch (const std::exception& error) { LogEditorError(engine, error); }
    }

    ImGui::End();
}

} // namespace PlagC::Editor
