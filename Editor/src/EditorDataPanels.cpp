#include "pgcpch.h"
#include "EditorDataPanels.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace PlagC::Editor {
namespace {

constexpr const char* CookedModelPayload = "PLAGC_COOKED_MODEL";

const char* KindName(EditorAssetKind kind)
{
    switch (kind)
    {
    case EditorAssetKind::Cooked: return "Cooked";
    case EditorAssetKind::ModelSource: return "Model";
    case EditorAssetKind::AudioSource: return "Audio";
    case EditorAssetKind::Texture: return "Texture";
    case EditorAssetKind::Shader: return "Shader";
    case EditorAssetKind::Scene: return "Scene";
    case EditorAssetKind::Material: return "Material";
    case EditorAssetKind::Other: return "Other";
    }
    return "Other";
}

const char* StateName(const std::optional<AssetState>& state)
{
    if (!state)
        return "Not loaded";
    switch (*state)
    {
    case AssetState::Queued: return "Queued";
    case AssetState::Loading: return "Loading";
    case AssetState::Ready: return "Ready";
    case AssetState::Failed: return "Failed";
    }
    return "Unknown";
}

const char* SeverityName(Severity severity)
{
    switch (severity)
    {
    case Severity::Trace: return "TRACE";
    case Severity::Debug: return "DEBUG";
    case Severity::Info: return "INFO";
    case Severity::Warning: return "WARN";
    case Severity::Error: return "ERROR";
    case Severity::Critical: return "CRITICAL";
    }
    return "?";
}

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool Matches(std::string_view value, const char* search)
{
    if (!search || !*search)
        return true;
    return Lower(std::string(value)).find(Lower(search)) != std::string::npos;
}

std::string Normalized(std::string value)
{
    std::replace(value.begin(), value.end(), '\\', '/');
    while (value.find("//") != std::string::npos && !value.starts_with("Assets://"))
        value.replace(value.find("//"), 2, "/");
    return value;
}

std::string RelativeAssetPath(std::string path)
{
    path = Normalized(std::move(path));
    constexpr std::string_view mounted = "Assets://";
    constexpr std::string_view filesystem = "Assets/";
    if (path.starts_with(mounted))
        return path.substr(mounted.size());
    if (path.starts_with(filesystem))
        return path.substr(filesystem.size());
    return path;
}

std::string ParentFolder(std::string relative)
{
    relative = Normalized(std::move(relative));
    const auto slash = relative.rfind('/');
    return slash == std::string::npos ? std::string{} : relative.substr(0, slash);
}

std::string AssetName(const AssetBrowserEntry& entry)
{
    const std::string relative = RelativeAssetPath(entry.path);
    const auto slash = relative.rfind('/');
    return slash == std::string::npos ? relative : relative.substr(slash + 1);
}

bool IsRevealMatch(std::string entryPath, std::string assetPath)
{
    entryPath = Normalized(std::move(entryPath));
    assetPath = Normalized(std::move(assetPath));
    if (entryPath == assetPath)
        return true;
    if (RelativeAssetPath(entryPath) == RelativeAssetPath(assetPath))
        return true;
    if (entryPath.size() <= assetPath.size())
        return false;
    return entryPath.ends_with('/' + assetPath);
}

EditorAssetKind EffectiveKind(const AssetBrowserEntry& entry)
{
    const std::string extension = Lower(std::filesystem::path(entry.path).extension().string());
    if (extension == ".pgmaterial" || extension == ".material" || extension == ".mat")
        return EditorAssetKind::Material;
    if (extension == ".plscene" || extension == ".scene")
        return EditorAssetKind::Scene;
    return entry.kind;
}

const char* RuntimeTypeName(const std::optional<AssetType>& type)
{
    if (!type) return "-";
    switch (*type)
    {
    case AssetType::Model: return "ModelAsset";
    case AssetType::Audio: return "AudioAsset";
    case AssetType::Texture: return "TextureAsset";
    }
    return "Unknown";
}

const char* OriginName(const AssetBrowserEntry& entry)
{
    const std::string relative = RelativeAssetPath(entry.path);
    if (relative.starts_with("Source/")) return "SOURCE";
    if (relative.starts_with("Cooked/") || EffectiveKind(entry) == EditorAssetKind::Cooked) return "COOKED";
    return "ASSET";
}

const char* KindGlyph(EditorAssetKind kind)
{
    switch (kind)
    {
    case EditorAssetKind::Cooked: return "[PKG]";
    case EditorAssetKind::ModelSource: return "[3D]";
    case EditorAssetKind::AudioSource: return "[AUD]";
    case EditorAssetKind::Texture: return "[TEX]";
    case EditorAssetKind::Shader: return "[SHD]";
    case EditorAssetKind::Scene: return "[SCN]";
    case EditorAssetKind::Material: return "[MAT]";
    case EditorAssetKind::Other: return "[FILE]";
    }
    return "[FILE]";
}

bool IsInternalAsset(const AssetBrowserEntry& entry)
{
    const std::string relative = RelativeAssetPath(entry.path);
    const std::string lower = Lower(relative);
    if (lower.starts_with("manifests/"))
        return true;
    if (lower.ends_with(".import.json") || lower.ends_with(".meta") ||
        lower.ends_with(".tmp") || lower.ends_with(".cache"))
        return true;

    std::size_t begin = 0;
    while (begin < relative.size())
    {
        const auto end = relative.find('/', begin);
        const auto component = relative.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
        if (!component.empty() && component.front() == '.')
            return true;
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return false;
}

bool IsCookedModel(const AssetBrowserEntry& entry)
{
    if (std::filesystem::path(entry.path).extension() != ".plasset")
        return false;
    if (entry.runtimeType && *entry.runtimeType != AssetType::Model)
        return false;
    return EffectiveKind(entry) == EditorAssetKind::Cooked ||
           RelativeAssetPath(entry.path).starts_with("Cooked/");
}

std::string FormatBytes(std::uint64_t bytes)
{
    constexpr double K = 1024.0;
    char buffer[64]{};
    if (bytes >= 1024ull * 1024ull * 1024ull)
        std::snprintf(buffer, sizeof(buffer), "%.2f GB", static_cast<double>(bytes) / (K*K*K));
    else if (bytes >= 1024ull * 1024ull)
        std::snprintf(buffer, sizeof(buffer), "%.2f MB", static_cast<double>(bytes) / (K*K));
    else if (bytes >= 1024ull)
        std::snprintf(buffer, sizeof(buffer), "%.1f KB", static_cast<double>(bytes) / K);
    else
        std::snprintf(buffer, sizeof(buffer), "%llu B", static_cast<unsigned long long>(bytes));
    return buffer;
}

std::vector<std::string> ImmediateFolders(const std::vector<AssetBrowserEntry>& entries,
                                          const std::string& folder,
                                          bool showInternal)
{
    std::set<std::string> folders;
    const std::string prefix = folder.empty() ? std::string{} : folder + '/';
    for (const auto& entry : entries)
    {
        if (!showInternal && IsInternalAsset(entry)) continue;
        const std::string relative = RelativeAssetPath(entry.path);
        if (!relative.starts_with(prefix)) continue;
        const std::string tail = relative.substr(prefix.size());
        const auto slash = tail.find('/');
        if (slash == std::string::npos) continue;
        const std::string child = tail.substr(0, slash);
        folders.insert(folder.empty() ? child : folder + '/' + child);
    }
    return {folders.begin(), folders.end()};
}

bool DirectAssetInFolder(const AssetBrowserEntry& entry, const std::string& folder)
{
    return ParentFolder(RelativeAssetPath(entry.path)) == folder;
}

bool AssetFilterMatches(const AssetBrowserEntry& entry, const char* search, int kindFilter,
                        bool showInternal)
{
    if (!showInternal && IsInternalAsset(entry)) return false;
    if (kindFilter >= 0 && static_cast<int>(EffectiveKind(entry)) != kindFilter) return false;
    return Matches(entry.path, search) || Matches(AssetName(entry), search);
}

void SortAssets(std::vector<const AssetBrowserEntry*>& assets, int sortMode)
{
    std::sort(assets.begin(), assets.end(), [sortMode](const AssetBrowserEntry* a, const AssetBrowserEntry* b)
    {
        const std::string an = Lower(AssetName(*a)), bn = Lower(AssetName(*b));
        switch (sortMode)
        {
        case 1: return an == bn ? a->path > b->path : an > bn;
        case 2: return a->bytes == b->bytes ? an < bn : a->bytes > b->bytes;
        case 3: return a->bytes == b->bytes ? an < bn : a->bytes < b->bytes;
        case 4:
        {
            const auto ak = static_cast<int>(EffectiveKind(*a)), bk = static_cast<int>(EffectiveKind(*b));
            return ak == bk ? an < bn : ak < bk;
        }
        default: return an == bn ? a->path < b->path : an < bn;
        }
    });
}

void DrawBreadcrumbs(const std::string& folder, const std::function<void(std::string)>& navigate)
{
    if (ImGui::SmallButton("Assets")) navigate({});
    if (folder.empty()) return;

    std::string accumulated;
    std::size_t begin = 0;
    while (begin < folder.size())
    {
        const auto end = folder.find('/', begin);
        const std::string component = folder.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
        accumulated = accumulated.empty() ? component : accumulated + '/' + component;
        ImGui::SameLine(); ImGui::TextDisabled(">"); ImGui::SameLine();
        const std::string id = component + "##crumb" + accumulated;
        if (ImGui::SmallButton(id.c_str())) navigate(accumulated);
        if (end == std::string::npos) break;
        begin = end + 1;
    }
}

void DrawAssetDetails(const AssetBrowserEntry& entry)
{
    ImGui::SeparatorText("Details");
    ImGui::TextWrapped("%s", AssetName(entry).c_str());
    ImGui::TextDisabled("%s", entry.path.c_str());
    ImGui::Spacing();
    ImGui::Text("Type: %s", KindName(EffectiveKind(entry)));
    ImGui::Text("Origin: %s", OriginName(entry));
    ImGui::Text("Runtime: %s", RuntimeTypeName(entry.runtimeType));
    ImGui::Text("State: %s", StateName(entry.state));
    ImGui::Text("Size: %s", FormatBytes(entry.bytes).c_str());
    if (entry.generation)
        ImGui::Text("Generation: %llu", static_cast<unsigned long long>(entry.generation));
    if (!entry.error.empty())
    {
        ImGui::Spacing();
        ImGui::TextWrapped("Error: %s", entry.error.c_str());
    }
    ImGui::Spacing();
    if (std::string_view(OriginName(entry)) == "SOURCE")
        ImGui::TextDisabled("Source asset. Import/reimport is handled by the Asset Import workflow.");
    else if (IsCookedModel(entry))
        ImGui::TextDisabled("Cooked ModelAsset. Drag to Viewport/Inspector or double-click to create an entity.");
    else if (EffectiveKind(entry) == EditorAssetKind::Scene)
        ImGui::TextDisabled("Scene opening remains owned by Milestone 16 Scene File Workflow.");
}

void DrawAssetContextMenu(const AssetBrowserEntry& entry,
                          const std::function<void(std::string)>& activate,
                          const std::function<void(std::string)>& navigate)
{
    if (!ImGui::BeginPopupContextItem()) return;
    if (ImGui::MenuItem("Copy Asset Path"))
        ImGui::SetClipboardText(entry.path.c_str());
    if (ImGui::MenuItem("Show in Folder"))
        navigate(ParentFolder(RelativeAssetPath(entry.path)));
    if (IsCookedModel(entry))
    {
        ImGui::Separator();
        if (ImGui::MenuItem("Create Model Entity"))
            activate(entry.path);
    }
    ImGui::EndPopup();
}

void DrawAssetDragSource(const AssetBrowserEntry& entry)
{
    if (!IsCookedModel(entry)) return;
    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
    {
        ImGui::SetDragDropPayload(CookedModelPayload, entry.path.c_str(), entry.path.size() + 1u);
        ImGui::Text("ModelAsset: %s", AssetName(entry).c_str());
        ImGui::TextDisabled("Drop into Viewport to place or Inspector to assign.");
        ImGui::EndDragDropSource();
    }
}

void DrawAssetCard(const AssetBrowserEntry& entry, float width, bool selected,
                   const std::function<void(std::string)>& select,
                   const std::function<void(std::string)>& activate,
                   const std::function<void(std::string)>& navigate)
{
    ImGui::PushID(entry.path.c_str());
    ImGui::BeginGroup();
    if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Header));
    const std::string button = std::string(KindGlyph(EffectiveKind(entry))) + "\n" + OriginName(entry);
    if (ImGui::Button(button.c_str(), ImVec2(width, 54.0f))) select(entry.path);
    if (selected) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && IsCookedModel(entry))
        activate(entry.path);
    DrawAssetDragSource(entry);
    DrawAssetContextMenu(entry, activate, navigate);
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width);
    ImGui::TextWrapped("%s", AssetName(entry).c_str());
    ImGui::PopTextWrapPos();
    ImGui::TextDisabled("%s", KindName(EffectiveKind(entry)));
    ImGui::EndGroup();
    ImGui::PopID();
}

} // namespace

void EditorDataPanels::Initialize(Engine& engine)
{
    RefreshAssets(engine);
}

void EditorDataPanels::RefreshAssets(Engine& engine)
{
    try
    {
        const auto generation = assets_.BeginRefresh();
        const std::array roots{AssetRoot{std::filesystem::path("Assets"), "Assets"}};
        auto entries = AssetBrowserModel::ScanFilesystem(roots);
        const auto registry = engine.Assets().Registry();
        AssetBrowserModel::MergeRegistry(entries, registry);
        assets_.Publish(generation, std::move(entries));
        engine.Logger().Write(Severity::Debug, "EDITOR-ASSETS", "Asset browser refreshed.");
    }
    catch (const std::exception& error)
    {
        engine.Logger().Write(Severity::Error, "EDITOR-ASSETS", error.what());
    }
}

void EditorDataPanels::NavigateContent(std::string folder, bool recordHistory)
{
    folder = RelativeAssetPath(std::move(folder));
    while (!folder.empty() && folder.back() == '/') folder.pop_back();
    if (folder == contentFolder_) return;
    contentFolder_ = std::move(folder);
    selectedAsset_.clear();
    if (!recordHistory) return;
    if (contentHistoryIndex_ + 1 < contentHistory_.size())
        contentHistory_.erase(contentHistory_.begin() + static_cast<std::ptrdiff_t>(contentHistoryIndex_ + 1), contentHistory_.end());
    contentHistory_.push_back(contentFolder_);
    contentHistoryIndex_ = contentHistory_.size() - 1;
}

void EditorDataPanels::QueueCookedModelActivation(std::string path)
{
    cookedModelActivation_ = Normalized(std::move(path));
}

void EditorDataPanels::RevealAsset(std::string path)
{
    revealAsset_ = Normalized(std::move(path));
    assetSearch_.fill('\0');
}

void EditorDataPanels::DrawContentBrowser(Engine& engine)
{
    const auto snapshot = assets_.Snapshot();
    if (!snapshot)
    {
        ImGui::TextDisabled("Scanning assets...");
        return;
    }

    if (!revealAsset_.empty())
    {
        const auto found = std::find_if(snapshot->begin(), snapshot->end(), [&](const AssetBrowserEntry& entry)
        {
            return IsRevealMatch(entry.path, revealAsset_);
        });
        if (found != snapshot->end())
        {
            NavigateContent(ParentFolder(RelativeAssetPath(found->path)));
            selectedAsset_ = found->path;
        }
        revealAsset_.clear();
    }

    ImGui::BeginDisabled(contentHistoryIndex_ == 0);
    if (ImGui::Button("Back##ContentBrowser"))
    {
        --contentHistoryIndex_;
        NavigateContent(contentHistory_[contentHistoryIndex_], false);
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(contentFolder_.empty());
    if (ImGui::Button("Up##ContentBrowser")) NavigateContent(ParentFolder(contentFolder_));
    ImGui::EndDisabled();
    ImGui::SameLine();
    DrawBreadcrumbs(contentFolder_, [&](std::string folder){ NavigateContent(std::move(folder)); });

    ImGui::SameLine();
    const float toolbarRight = ImGui::GetWindowContentRegionMax().x;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), toolbarRight - 72.0f));
    if (ImGui::Button("Refresh")) RefreshAssets(engine);

    ImGui::SetNextItemWidth(std::min(300.0f, std::max(120.0f, ImGui::GetContentRegionAvail().x * 0.35f)));
    ImGui::InputTextWithHint("##asset-search", "Search assets...", assetSearch_.data(), assetSearch_.size());
    ImGui::SameLine();

    const char* filterLabel = contentKindFilter_ < 0 ? "All Types" : KindName(static_cast<EditorAssetKind>(contentKindFilter_));
    ImGui::SetNextItemWidth(105.0f);
    if (ImGui::BeginCombo("##ContentTypeFilter", filterLabel))
    {
        if (ImGui::Selectable("All Types", contentKindFilter_ < 0)) contentKindFilter_ = -1;
        for (EditorAssetKind kind : {EditorAssetKind::Cooked, EditorAssetKind::ModelSource,
             EditorAssetKind::Texture, EditorAssetKind::AudioSource, EditorAssetKind::Material,
             EditorAssetKind::Scene, EditorAssetKind::Shader, EditorAssetKind::Other})
            if (ImGui::Selectable(KindName(kind), contentKindFilter_ == static_cast<int>(kind)))
                contentKindFilter_ = static_cast<int>(kind);
        ImGui::EndCombo();
    }
    ImGui::SameLine();

    constexpr const char* SortNames[]{"Name A-Z", "Name Z-A", "Largest", "Smallest", "Type"};
    const int sortIndex = static_cast<int>(contentSort_);
    ImGui::SetNextItemWidth(105.0f);
    if (ImGui::BeginCombo("##ContentSort", SortNames[sortIndex]))
    {
        for (int i=0;i<5;++i)
            if (ImGui::Selectable(SortNames[i], sortIndex == i)) contentSort_ = static_cast<ContentSort>(i);
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    if (ImGui::Button(contentGridView_ ? "List" : "Grid")) contentGridView_ = !contentGridView_;
    ImGui::SameLine();
    ImGui::Checkbox("Internal", &contentShowInternal_);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Show import receipts, manifests, dotfiles and other editor/internal asset metadata.");

    ImGui::Separator();

    const bool searching = assetSearch_[0] != '\0';
    const auto folders = searching ? std::vector<std::string>{} : ImmediateFolders(*snapshot, contentFolder_, contentShowInternal_);
    std::vector<const AssetBrowserEntry*> visibleAssets;
    visibleAssets.reserve(snapshot->size());
    for (const auto& entry : *snapshot)
    {
        if (!AssetFilterMatches(entry, assetSearch_.data(), contentKindFilter_, contentShowInternal_)) continue;
        if (!searching && !DirectAssetInFolder(entry, contentFolder_)) continue;
        visibleAssets.push_back(&entry);
    }
    SortAssets(visibleAssets, static_cast<int>(contentSort_));

    const float detailsWidth = ImGui::GetContentRegionAvail().x > 620.0f ? 245.0f : 0.0f;
    if (ImGui::BeginChild("ContentBrowserItems", ImVec2(detailsWidth > 0 ? -detailsWidth - 8.0f : 0.0f, 0.0f), false))
    {
        if (contentGridView_)
        {
            const float cardWidth = 116.0f;
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const int columns = std::max(1, static_cast<int>((ImGui::GetContentRegionAvail().x + spacing) / (cardWidth + spacing)));
            int column = 0;
            for (const auto& folder : folders)
            {
                ImGui::PushID(folder.c_str());
                ImGui::BeginGroup();
                if (ImGui::Button("[DIR]", ImVec2(cardWidth,54.0f))) NavigateContent(folder);
                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) NavigateContent(folder);
                ImGui::TextWrapped("%s", std::filesystem::path(folder).filename().string().c_str());
                ImGui::TextDisabled("Folder");
                ImGui::EndGroup();
                ImGui::PopID();
                if (++column % columns) ImGui::SameLine();
            }
            for (const auto* entry : visibleAssets)
            {
                DrawAssetCard(*entry, cardWidth, selectedAsset_ == entry->path,
                    [&](std::string path){ selectedAsset_ = std::move(path); },
                    [&](std::string path){ QueueCookedModelActivation(std::move(path)); },
                    [&](std::string folder){ NavigateContent(std::move(folder)); });
                if (++column % columns) ImGui::SameLine();
            }
        }
        else
        {
            constexpr ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("ContentBrowserList", 5, flags, ImVec2(0,0)))
            {
                ImGui::TableSetupScrollFreeze(0,1);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, .45f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 80.0f);
                ImGui::TableSetupColumn("Origin", ImGuiTableColumnFlags_WidthFixed, 70.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 75.0f);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 75.0f);
                ImGui::TableHeadersRow();
                for (const auto& folder : folders)
                {
                    ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
                    const std::string label = "[DIR] " + std::filesystem::path(folder).filename().string() + "##" + folder;
                    if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) NavigateContent(folder);
                    ImGui::TableSetColumnIndex(1); ImGui::TextDisabled("Folder");
                }
                for (const auto* entry : visibleAssets)
                {
                    ImGui::PushID(entry->path.c_str());
                    ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
                    const std::string label = std::string(KindGlyph(EffectiveKind(*entry))) + " " + AssetName(*entry);
                    if (ImGui::Selectable(label.c_str(), selectedAsset_ == entry->path,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick))
                    {
                        selectedAsset_ = entry->path;
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && IsCookedModel(*entry))
                            QueueCookedModelActivation(entry->path);
                    }
                    DrawAssetDragSource(*entry);
                    DrawAssetContextMenu(*entry,
                        [&](std::string path){ QueueCookedModelActivation(std::move(path)); },
                        [&](std::string folder){ NavigateContent(std::move(folder)); });
                    ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(KindName(EffectiveKind(*entry)));
                    ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(OriginName(*entry));
                    ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(StateName(entry->state));
                    ImGui::TableSetColumnIndex(4); ImGui::TextUnformatted(FormatBytes(entry->bytes).c_str());
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }

        if (folders.empty() && visibleAssets.empty())
            ImGui::TextDisabled(searching ? "No assets match the current search/filter." : "This folder contains no visible assets.");
    }
    ImGui::EndChild();

    if (detailsWidth > 0.0f)
    {
        ImGui::SameLine();
        if (ImGui::BeginChild("ContentBrowserDetails", ImVec2(detailsWidth,0), true))
        {
            const auto selected = std::find_if(snapshot->begin(), snapshot->end(), [&](const AssetBrowserEntry& entry)
            {
                return entry.path == selectedAsset_;
            });
            if (selected != snapshot->end()) DrawAssetDetails(*selected);
            else
            {
                ImGui::SeparatorText("Details");
                ImGui::TextDisabled("Select an asset to inspect its type, origin, load state and size.");
            }
        }
        ImGui::EndChild();
    }
}

void EditorDataPanels::DrawConsole(Engine& engine)
{
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::InputTextWithHint("##console-search", "Search logs...",
                                 consoleSearch_.data(), consoleSearch_.size()))
        consoleFilter_.search = consoleSearch_.data();
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        engine.Logger().Clear();
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScrollConsole_);

    ImGui::Checkbox("Trace", &consoleFilter_.trace); ImGui::SameLine();
    ImGui::Checkbox("Debug", &consoleFilter_.debug); ImGui::SameLine();
    ImGui::Checkbox("Info", &consoleFilter_.info); ImGui::SameLine();
    ImGui::Checkbox("Warn", &consoleFilter_.warning); ImGui::SameLine();
    ImGui::Checkbox("Error", &consoleFilter_.error); ImGui::SameLine();
    ImGui::Checkbox("Critical", &consoleFilter_.critical);

    const auto all = engine.Logger().Snapshot();
    const auto records = FilterLogRecords(all, consoleFilter_);

    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_ScrollX;
    if (!ImGui::BeginTable("ConsoleTable", 4, flags, ImVec2(0.0f, 0.0f)))
        return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 95.0f);
    ImGui::TableSetupColumn("Level", ImGuiTableColumnFlags_WidthFixed, 70.0f);
    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch, 0.75f);
    ImGui::TableHeadersRow();

    const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2.0f;
    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(records.size()));
    while (clipper.Step())
    {
        for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
        {
            const auto& record = records[static_cast<std::size_t>(index)];
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(record.timestamp.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::TextUnformatted(SeverityName(record.severity));
            ImGui::TableSetColumnIndex(2); ImGui::TextUnformatted(record.category.c_str());
            ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(record.message.c_str());
        }
    }

    if (autoScrollConsole_ && atBottom)
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndTable();
}

} // namespace PlagC::Editor
