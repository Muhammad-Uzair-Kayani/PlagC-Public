#include "pgcpch.h"
#include "EditorDataPanels.h"
#include <PlagC/Application.h>
#include <PlagC/Assets/AssetImport.h>
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace PlagC::Editor {
namespace {

constexpr const char* CookedModelPayload = "PLAGC_COOKED_MODEL";

enum class BrowserKind
{
    Model,
    Texture,
    Audio,
    Material,
    Scene,
    Controller,
    Shader,
    Cooked,
    Other
};

enum class BrowserOrigin
{
    Source,
    Cooked,
    Project
};

struct BrowserAsset
{
    AssetBrowserEntry entry;
    std::string path;
    BrowserKind kind = BrowserKind::Other;
    BrowserOrigin origin = BrowserOrigin::Source;
};

struct BrowserItem
{
    bool folder = false;
    std::string path;
    std::string name;
    const BrowserAsset* asset = nullptr;
};

struct SourceCookedPair
{
    std::string source;
    std::string cooked;
    AssetImportKind kind = AssetImportKind::Model;
};

std::string Lower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string Normalized(std::string value)
{
    std::replace(value.begin(), value.end(), '\\', '/');
    while (value.starts_with("./"))
        value.erase(0, 2);
    return value;
}

std::string CanonicalBrowserPath(std::string path)
{
    path = Normalized(std::move(path));
    if (path.starts_with("Assets://"))
        return path;
    if (path.starts_with("Assets/"))
        return "Assets://" + path.substr(std::string_view("Assets/").size());
    if (path.starts_with("Cooked/"))
        return "Assets://Cooked/" + path.substr(std::string_view("Cooked/").size());
    if (path.find("://") != std::string::npos)
        return path;
    return "Assets://Cooked/" + path;
}

std::filesystem::path DiskPath(std::string path)
{
    path = CanonicalBrowserPath(std::move(path));
    constexpr std::string_view prefix = "Assets://";
    if (!path.starts_with(prefix))
        return {};
    return std::filesystem::path("Assets") / path.substr(prefix.size());
}

std::string MountedPath(const std::filesystem::path& disk)
{
    std::string path = Normalized(disk.lexically_normal().generic_string());
    if (path.starts_with("Assets/"))
        return "Assets://" + path.substr(std::string_view("Assets/").size());

    const std::string marker = "/Assets/";
    if (const auto found = path.find(marker); found != std::string::npos)
        return "Assets://" + path.substr(found + marker.size());

    return path;
}

std::string BrowserOrDiskPath(const std::filesystem::path& disk)
{
    const std::string mounted = MountedPath(disk);
    if (mounted.starts_with("Assets://"))
        return mounted;
    return Normalized(disk.lexically_normal().generic_string());
}

std::string CookedAssetKey(std::string path)
{
    path = Normalized(std::move(path));
    constexpr std::string_view mountedPrefix = "Assets://Cooked/";
    constexpr std::string_view filesystemPrefix = "Assets/Cooked/";
    constexpr std::string_view cookedPrefix = "Cooked/";
    if (path.starts_with(mountedPrefix))
        return path.substr(mountedPrefix.size());
    if (path.starts_with(filesystemPrefix))
        return path.substr(filesystemPrefix.size());
    if (path.starts_with(cookedPrefix))
        return path.substr(cookedPrefix.size());
    return path;
}

std::string LeafName(std::string_view path)
{
    if (path == "Assets://")
        return "Assets";
    while (!path.empty() && path.back() == '/')
        path.remove_suffix(1);
    const auto slash = path.find_last_of('/');
    return std::string(slash == std::string_view::npos ? path : path.substr(slash + 1));
}

std::string ParentFolder(std::string path)
{
    path = CanonicalBrowserPath(std::move(path));
    if (path == "Assets://")
        return path;
    while (path.size() > std::string_view("Assets://").size() && path.back() == '/')
        path.pop_back();
    const auto slash = path.find_last_of('/');
    if (slash == std::string::npos || slash < std::string_view("Assets://").size())
        return "Assets://";
    return path.substr(0, slash);
}

std::string FolderPrefix(std::string folder)
{
    folder = CanonicalBrowserPath(std::move(folder));
    if (!folder.ends_with('/'))
        folder.push_back('/');
    return folder;
}

BrowserOrigin OriginFor(std::string_view path, BrowserKind kind)
{
    if (path.starts_with("Assets://Cooked/"))
        return BrowserOrigin::Cooked;
    if (kind == BrowserKind::Scene || kind == BrowserKind::Controller ||
        kind == BrowserKind::Material)
        return BrowserOrigin::Project;
    return BrowserOrigin::Source;
}

BrowserKind KindFor(const AssetBrowserEntry& entry, std::string_view path)
{
    if (entry.runtimeType)
    {
        switch (*entry.runtimeType)
        {
        case AssetType::Model: return BrowserKind::Model;
        case AssetType::Audio: return BrowserKind::Audio;
        case AssetType::Texture: return BrowserKind::Texture;
        }
    }

    const std::string extension = Lower(std::filesystem::path(path).extension().string());
    if (extension == ".pgmaterial")
        return BrowserKind::Material;
    if (extension == ".pgcontroller")
        return BrowserKind::Controller;
    if (extension == ".plscene" || extension == ".scene")
        return BrowserKind::Scene;

    switch (entry.kind)
    {
    case EditorAssetKind::ModelSource: return BrowserKind::Model;
    case EditorAssetKind::AudioSource: return BrowserKind::Audio;
    case EditorAssetKind::Texture: return BrowserKind::Texture;
    case EditorAssetKind::Shader: return BrowserKind::Shader;
    case EditorAssetKind::Scene: return BrowserKind::Scene;
    case EditorAssetKind::Material: return BrowserKind::Material;
    case EditorAssetKind::Cooked: return BrowserKind::Cooked;
    case EditorAssetKind::Other: return BrowserKind::Other;
    }
    return BrowserKind::Other;
}

const char* KindName(BrowserKind kind)
{
    switch (kind)
    {
    case BrowserKind::Model: return "Model";
    case BrowserKind::Texture: return "Texture";
    case BrowserKind::Audio: return "Audio";
    case BrowserKind::Material: return "Material";
    case BrowserKind::Scene: return "Scene";
    case BrowserKind::Controller: return "Controller";
    case BrowserKind::Shader: return "Shader";
    case BrowserKind::Cooked: return "Cooked";
    case BrowserKind::Other: return "Other";
    }
    return "Other";
}

const char* KindBadge(BrowserKind kind)
{
    switch (kind)
    {
    case BrowserKind::Model: return "MDL";
    case BrowserKind::Texture: return "TEX";
    case BrowserKind::Audio: return "AUD";
    case BrowserKind::Material: return "MAT";
    case BrowserKind::Scene: return "SCN";
    case BrowserKind::Controller: return "CTRL";
    case BrowserKind::Shader: return "SHD";
    case BrowserKind::Cooked: return "CKD";
    case BrowserKind::Other: return "FILE";
    }
    return "FILE";
}

const char* OriginName(BrowserOrigin origin)
{
    switch (origin)
    {
    case BrowserOrigin::Source: return "Source";
    case BrowserOrigin::Cooked: return "Cooked";
    case BrowserOrigin::Project: return "Project";
    }
    return "?";
}

const char* StateName(const std::optional<AssetState>& state)
{
    if (!state)
        return "-";
    switch (*state)
    {
    case AssetState::Queued: return "Queued";
    case AssetState::Loading: return "Loading";
    case AssetState::Ready: return "Ready";
    case AssetState::Failed: return "Failed";
    }
    return "?";
}

const char* RuntimeTypeName(const std::optional<AssetType>& type)
{
    if (!type)
        return "-";
    switch (*type)
    {
    case AssetType::Model: return "Model";
    case AssetType::Audio: return "Audio";
    case AssetType::Texture: return "Texture";
    }
    return "?";
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

std::string HumanBytes(std::uint64_t bytes)
{
    char buffer[64]{};
    if (bytes >= 1024ull * 1024ull * 1024ull)
        std::snprintf(buffer, sizeof(buffer), "%.2f GiB",
                      static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    else if (bytes >= 1024ull * 1024ull)
        std::snprintf(buffer, sizeof(buffer), "%.2f MiB",
                      static_cast<double>(bytes) / (1024.0 * 1024.0));
    else if (bytes >= 1024ull)
        std::snprintf(buffer, sizeof(buffer), "%.1f KiB",
                      static_cast<double>(bytes) / 1024.0);
    else
        std::snprintf(buffer, sizeof(buffer), "%llu B",
                      static_cast<unsigned long long>(bytes));
    return buffer;
}

bool IsInternalAsset(std::string_view path)
{
    const std::string lowered = Lower(std::string(path));
    if (lowered.starts_with("assets://manifests/") ||
        lowered.starts_with("assets://shaders/") ||
        lowered.starts_with("assets://compositors/"))
        return true;

    const std::filesystem::path file(path);
    const std::string name = Lower(file.filename().string());
    if (name.empty() || name.front() == '.' || name.front() == '_')
        return true;
    if (name.ends_with(".import") || name.ends_with(".tmp") ||
        name.ends_with(".lock.json") || name == "manifest.json")
        return true;
    if (name.find("license") != std::string::npos)
        return true;
    const std::string extension = Lower(file.extension().string());
    return extension == ".md" || extension == ".txt";
}

bool Matches(std::string_view value, const char* search)
{
    if (!search || !*search)
        return true;
    return Lower(std::string(value)).find(Lower(search)) != std::string::npos;
}

bool MatchesAssetSearch(const BrowserAsset& asset, const char* search)
{
    return Matches(asset.path, search) || Matches(LeafName(asset.path), search) ||
           Matches(KindName(asset.kind), search);
}

bool PassesOrigin(const BrowserAsset& asset, int filter)
{
    return filter == 0 ||
           (filter == 1 && asset.origin == BrowserOrigin::Source) ||
           (filter == 2 && asset.origin == BrowserOrigin::Cooked) ||
           (filter == 3 && asset.origin == BrowserOrigin::Project);
}

bool PassesType(const BrowserAsset& asset, int filter)
{
    if (filter == 0)
        return true;
    const BrowserKind requested = static_cast<BrowserKind>(filter - 1);
    return asset.kind == requested;
}

bool IsRevealMatch(std::string entryPath, std::string assetPath)
{
    entryPath = Normalized(std::move(entryPath));
    assetPath = Normalized(std::move(assetPath));
    if (entryPath == assetPath)
        return true;
    if (entryPath.size() <= assetPath.size())
        return false;
    return entryPath.ends_with('/' + assetPath);
}

std::vector<BrowserAsset>
MergeBrowserEntries(const std::shared_ptr<const std::vector<AssetBrowserEntry>>& snapshot)
{
    std::vector<BrowserAsset> result;
    if (!snapshot)
        return result;

    std::unordered_map<std::string, std::size_t> byPath;
    result.reserve(snapshot->size());
    byPath.reserve(snapshot->size());

    for (const auto& source : *snapshot)
    {
        const std::string path = CanonicalBrowserPath(source.path);
        const auto found = byPath.find(path);
        if (found == byPath.end())
        {
            BrowserAsset item;
            item.entry = source;
            item.entry.path = path;
            item.path = path;
            item.kind = KindFor(item.entry, path);
            item.origin = OriginFor(path, item.kind);
            byPath.emplace(path, result.size());
            result.push_back(std::move(item));
            continue;
        }

        auto& item = result[found->second];
        if (source.state)
            item.entry.state = source.state;
        if (source.runtimeType)
            item.entry.runtimeType = source.runtimeType;
        if (source.bytes)
            item.entry.bytes = source.bytes;
        if (source.generation)
            item.entry.generation = source.generation;
        if (!source.error.empty())
            item.entry.error = source.error;
        item.kind = KindFor(item.entry, item.path);
        item.origin = OriginFor(item.path, item.kind);
    }

    return result;
}

std::optional<SourceCookedPair> PairFromReceipt(const std::filesystem::path& cooked)
{
    try
    {
        if (!std::filesystem::is_regular_file(ImportReceiptPath(cooked)))
            return std::nullopt;
        const AssetImportReceipt receipt = LoadAssetImportReceipt(cooked);
        SourceCookedPair pair;
        pair.source = BrowserOrDiskPath(receipt.source);
        pair.cooked = BrowserOrDiskPath(receipt.cooked);
        pair.kind = receipt.kind;
        return pair;
    }
    catch (...)
    {
        return std::nullopt;
    }
}

std::optional<SourceCookedPair>
FindPair(std::string_view selectedPath, const std::vector<BrowserAsset>& assets)
{
    const std::string selected = CanonicalBrowserPath(std::string(selectedPath));
    if (selected.starts_with("Assets://Cooked/"))
        return PairFromReceipt(DiskPath(selected));

    const std::string selectedDisk = Normalized(DiskPath(selected).lexically_normal().generic_string());
    for (const auto& asset : assets)
    {
        if (asset.origin != BrowserOrigin::Cooked ||
            Lower(std::filesystem::path(asset.path).extension().string()) != ".plasset")
            continue;
        const auto pair = PairFromReceipt(DiskPath(asset.path));
        if (!pair)
            continue;
        const std::string sourceDisk =
            Normalized(DiskPath(pair->source).lexically_normal().generic_string());
        if (sourceDisk == selectedDisk)
            return pair;
    }
    return std::nullopt;
}

bool CanDragAsCookedModel(const BrowserAsset& asset)
{
    if (asset.origin != BrowserOrigin::Cooked)
        return false;
    if (Lower(std::filesystem::path(asset.path).extension().string()) != ".plasset")
        return false;
    if (asset.entry.runtimeType)
        return *asset.entry.runtimeType == AssetType::Model;
    const auto receipt = PairFromReceipt(DiskPath(asset.path));
    return receipt && receipt->kind == AssetImportKind::Model;
}

void DrawModelDragSource(const BrowserAsset& asset)
{
    if (!CanDragAsCookedModel(asset) ||
        !ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        return;

    ImGui::SetDragDropPayload(CookedModelPayload,
                              asset.path.c_str(), asset.path.size() + 1u);
    ImGui::Text("Model: %s", LeafName(asset.path).c_str());
    ImGui::TextDisabled("%s", asset.path.c_str());
    ImGui::EndDragDropSource();
}

std::vector<BrowserItem>
BuildItems(const std::vector<BrowserAsset>& assets,
           const std::string& folder,
           const char* search,
           int originFilter,
           int typeFilter,
           bool showInternal)
{
    std::vector<BrowserItem> items;
    const std::string prefix = FolderPrefix(folder);
    const bool searching = search && *search;
    std::unordered_set<std::string> folders;

    for (const auto& asset : assets)
    {
        if (!asset.path.starts_with(prefix))
            continue;
        if (!showInternal && IsInternalAsset(asset.path))
            continue;
        if (!PassesOrigin(asset, originFilter) || !PassesType(asset, typeFilter))
            continue;
        if (searching && !MatchesAssetSearch(asset, search))
            continue;

        const std::string relative = asset.path.substr(prefix.size());
        if (searching)
        {
            items.push_back({false, asset.path, LeafName(asset.path), &asset});
            continue;
        }

        const auto slash = relative.find('/');
        if (slash != std::string::npos)
        {
            const std::string child = prefix + relative.substr(0, slash);
            if (folders.insert(child).second)
                items.push_back({true, child, relative.substr(0, slash), nullptr});
        }
        else
            items.push_back({false, asset.path, LeafName(asset.path), &asset});
    }
    return items;
}

void SortItems(std::vector<BrowserItem>& items, int sortMode, bool ascending)
{
    std::sort(items.begin(), items.end(),
              [=](const BrowserItem& a, const BrowserItem& b)
              {
                  if (a.folder != b.folder)
                      return a.folder;

                  int comparison = 0;
                  if (sortMode == 1 && !a.folder && a.asset && b.asset)
                      comparison = std::string(KindName(a.asset->kind)).compare(KindName(b.asset->kind));
                  else if (sortMode == 2 && !a.folder && a.asset && b.asset)
                  {
                      if (a.asset->entry.bytes < b.asset->entry.bytes) comparison = -1;
                      else if (a.asset->entry.bytes > b.asset->entry.bytes) comparison = 1;
                  }

                  if (comparison == 0)
                      comparison = Lower(a.name).compare(Lower(b.name));
                  return ascending ? comparison < 0 : comparison > 0;
              });
}

const BrowserAsset* FindAsset(const std::vector<BrowserAsset>& assets,
                              std::string_view path)
{
    const auto found = std::find_if(assets.begin(), assets.end(),
        [&](const BrowserAsset& asset) { return asset.path == path; });
    return found == assets.end() ? nullptr : &*found;
}

void DrawBreadcrumbs(std::string& folder)
{
    if (ImGui::SmallButton("Assets"))
        folder = "Assets://";

    std::string relative = folder;
    if (relative.starts_with("Assets://"))
        relative.erase(0, std::string_view("Assets://").size());

    std::string assembled = "Assets://";
    std::size_t cursor = 0;
    while (cursor < relative.size())
    {
        const auto slash = relative.find('/', cursor);
        const std::string segment = relative.substr(
            cursor, slash == std::string::npos ? std::string::npos : slash - cursor);
        if (!segment.empty())
        {
            if (assembled != "Assets://")
                assembled.push_back('/');
            assembled += segment;
            ImGui::SameLine();
            ImGui::TextDisabled("/");
            ImGui::SameLine();
            ImGui::PushID(assembled.c_str());
            if (ImGui::SmallButton(segment.c_str()))
                folder = assembled;
            ImGui::PopID();
        }
        if (slash == std::string::npos)
            break;
        cursor = slash + 1;
    }
}

bool DrawItemContext(Engine& engine,
                     const BrowserItem& item,
                     const std::vector<BrowserAsset>& assets,
                     std::string& folder,
                     std::string& selected,
                     std::string& reveal)
{
    bool refresh = false;
    if (!ImGui::BeginPopupContextItem(item.folder ? "FolderContext" : "AssetContext"))
        return false;

    if (item.folder)
    {
        if (ImGui::MenuItem("Open"))
            folder = item.path;
        if (ImGui::MenuItem("Copy Path"))
            ImGui::SetClipboardText(item.path.c_str());
        ImGui::Separator();
        if (ImGui::MenuItem("Refresh"))
            refresh = true;
    }
    else if (item.asset)
    {
        if (ImGui::MenuItem("Select"))
            selected = item.path;
        if (ImGui::MenuItem("Copy Path"))
            ImGui::SetClipboardText(item.path.c_str());

        const auto pair = FindPair(item.path, assets);
        if (pair)
        {
            if (pair->source.starts_with("Assets://") &&
                item.path != pair->source && ImGui::MenuItem("Reveal Source"))
                reveal = pair->source;
            if (pair->cooked.starts_with("Assets://") &&
                item.path != pair->cooked && ImGui::MenuItem("Reveal Cooked"))
                reveal = pair->cooked;
        }

        if (item.asset->origin == BrowserOrigin::Cooked && item.asset->entry.state)
        {
            ImGui::Separator();
            if (ImGui::MenuItem("Reload Runtime Asset"))
            {
                try
                {
                    engine.Assets().Reload(CookedAssetKey(item.asset->path));
                    engine.Assets().Wait();
                    refresh = true;
                    engine.Logger().Write(
                        Severity::Info, "EDITOR-ASSETS",
                        "Reloaded runtime asset: " + item.asset->path);
                }
                catch (const std::exception& error)
                {
                    engine.Logger().Write(Severity::Error, "EDITOR-ASSETS", error.what());
                }
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Refresh"))
            refresh = true;
    }

    ImGui::EndPopup();
    return refresh;
}

void DrawDetails(const std::vector<BrowserAsset>& assets,
                 const std::string& selected,
                 std::string& reveal)
{
    ImGui::SeparatorText("Details");
    if (selected.empty())
    {
        ImGui::TextDisabled("Select an asset to inspect its details.");
        return;
    }

    const BrowserAsset* asset = FindAsset(assets, selected);
    if (!asset)
    {
        ImGui::TextDisabled("The selected asset is no longer present. Refresh the browser.");
        return;
    }

    ImGui::Text("%s  %s", KindBadge(asset->kind), LeafName(asset->path).c_str());
    ImGui::TextWrapped("%s", asset->path.c_str());
    ImGui::Text("Type: %s    Origin: %s", KindName(asset->kind), OriginName(asset->origin));
    ImGui::Text("Size: %s", HumanBytes(asset->entry.bytes).c_str());

    if (asset->entry.state)
    {
        ImGui::Text("Runtime: %s / %s",
                    RuntimeTypeName(asset->entry.runtimeType),
                    StateName(asset->entry.state));
        ImGui::Text("Generation: %llu",
                    static_cast<unsigned long long>(asset->entry.generation));
    }

    if (!asset->entry.error.empty())
    {
        ImGui::TextUnformatted("Asset error:");
        ImGui::TextWrapped("%s", asset->entry.error.c_str());
    }

    if (const auto pair = FindPair(asset->path, assets))
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Source / Cooked relationship");
        ImGui::TextWrapped("Source: %s", pair->source.c_str());
        ImGui::SameLine();
        if (pair->source.starts_with("Assets://") &&
            asset->path != pair->source && ImGui::SmallButton("Reveal Source"))
            reveal = pair->source;

        ImGui::TextWrapped("Cooked: %s", pair->cooked.c_str());
        ImGui::SameLine();
        if (pair->cooked.starts_with("Assets://") &&
            asset->path != pair->cooked && ImGui::SmallButton("Reveal Cooked"))
            reveal = pair->cooked;
    }
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

void EditorDataPanels::RevealAsset(std::string path)
{
    revealAsset_ = Normalized(std::move(path));
    assetSearch_.fill('\0');
}

std::string EditorDataPanels::ConsumeCookedModelActivation()
{
    std::string request = std::move(cookedModelActivation_);
    cookedModelActivation_.clear();
    return request;
}

void EditorDataPanels::DrawContentBrowser(Engine& engine)
{
    const auto snapshot = assets_.Snapshot();
    auto assets = MergeBrowserEntries(snapshot);

    bool revealNow = false;
    if (!revealAsset_.empty())
    {
        const std::string canonical = CanonicalBrowserPath(revealAsset_);
        const auto found = std::find_if(assets.begin(), assets.end(),
            [&](const BrowserAsset& asset)
            {
                return asset.path == canonical || IsRevealMatch(asset.path, revealAsset_);
            });
        if (found != assets.end())
        {
            selectedAsset_ = found->path;
            browserFolder_ = ParentFolder(found->path);
            assetSearch_.fill('\0');
            revealNow = true;
        }
        revealAsset_.clear();
    }

    ImGui::BeginDisabled(browserFolder_ == "Assets://");
    if (ImGui::Button("Up"))
        browserFolder_ = ParentFolder(browserFolder_);
    ImGui::EndDisabled();
    ImGui::SameLine();
    DrawBreadcrumbs(browserFolder_);

    ImGui::SetNextItemWidth(250.0f);
    ImGui::InputTextWithHint("##asset-search", "Search current folder...",
                             assetSearch_.data(), assetSearch_.size());
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        RefreshAssets(engine);
    ImGui::SameLine();
    if (ImGui::Button(assetGridView_ ? "List" : "Grid"))
        assetGridView_ = !assetGridView_;

    static constexpr const char* originItems[] = {"All origins", "Source", "Cooked", "Project"};
    static constexpr const char* typeItems[] = {
        "All types", "Model", "Texture", "Audio", "Material",
        "Scene", "Controller", "Shader", "Cooked", "Other"};
    static constexpr const char* sortItems[] = {"Name", "Type", "Size"};

    ImGui::SetNextItemWidth(120.0f);
    ImGui::Combo("##AssetOrigin", &assetOriginFilter_, originItems,
                 static_cast<int>(std::size(originItems)));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::Combo("##AssetType", &assetTypeFilter_, typeItems,
                 static_cast<int>(std::size(typeItems)));
    ImGui::SameLine();
    ImGui::SetNextItemWidth(95.0f);
    ImGui::Combo("##AssetSort", &assetSort_, sortItems,
                 static_cast<int>(std::size(sortItems)));
    ImGui::SameLine();
    if (ImGui::SmallButton(assetSortAscending_ ? "A-Z" : "Z-A"))
        assetSortAscending_ = !assetSortAscending_;
    ImGui::SameLine();
    ImGui::Checkbox("Show Internal", &showInternalAssets_);

    if (assetGridView_)
    {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(90.0f);
        ImGui::SliderFloat("Card", &assetCardWidth_, 96.0f, 196.0f, "%.0f px");
    }

    ImGui::Separator();

    if (assets.empty())
    {
        ImGui::TextDisabled("No assets discovered under Assets/.");
        return;
    }

    auto items = BuildItems(assets, browserFolder_, assetSearch_.data(),
                            assetOriginFilter_, assetTypeFilter_, showInternalAssets_);
    SortItems(items, assetSort_, assetSortAscending_);

    const float detailsHeight = 142.0f;
    if (ImGui::BeginChild("ContentBrowserItems", ImVec2(0.0f, -detailsHeight), false))
    {
        if (items.empty())
            ImGui::TextDisabled("No assets match the current folder and filters.");

        if (assetGridView_)
        {
            const float spacing = ImGui::GetStyle().ItemSpacing.x;
            const float available = std::max(assetCardWidth_, ImGui::GetContentRegionAvail().x);
            const int columns = std::max(1, static_cast<int>(
                (available + spacing) / (assetCardWidth_ + spacing)));

            if (ImGui::BeginTable("ContentBrowserGrid", columns,
                                  ImGuiTableFlags_SizingFixedFit))
            {
                for (const auto& item : items)
                {
                    ImGui::TableNextColumn();
                    ImGui::PushID(item.path.c_str());
                    const bool selected = !item.folder && selectedAsset_ == item.path;
                    std::string label = item.folder
                        ? "[DIR]\n" + item.name
                        : std::string("[") + KindBadge(item.asset->kind) + "]\n" + item.name;
                    const bool activated = ImGui::Selectable(
                        label.c_str(), selected,
                        ImGuiSelectableFlags_AllowDoubleClick,
                        ImVec2(assetCardWidth_, 58.0f));

                    if (activated && !item.folder)
                        selectedAsset_ = item.path;
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", item.path.c_str());

                    if (ImGui::IsItemHovered() &&
                        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        if (item.folder)
                            browserFolder_ = item.path;
                        else
                        {
                            selectedAsset_ = item.path;
                            if (item.asset && CanDragAsCookedModel(*item.asset))
                                cookedModelActivation_ = item.path;
                            else if (const auto pair = FindPair(item.path, assets))
                            {
                                const std::string counterpart =
                                    item.path == pair->source ? pair->cooked : pair->source;
                                if (counterpart.starts_with("Assets://"))
                                    revealAsset_ = counterpart;
                            }
                        }
                    }

                    if (!item.folder && item.asset)
                        DrawModelDragSource(*item.asset);
                    if (DrawItemContext(engine, item, assets, browserFolder_,
                                        selectedAsset_, revealAsset_))
                        RefreshAssets(engine);

                    if (revealNow && !item.folder && selectedAsset_ == item.path)
                        ImGui::SetScrollHereY(0.5f);
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        else
        {
            constexpr ImGuiTableFlags flags =
                ImGuiTableFlags_RowBg |
                ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_Resizable |
                ImGuiTableFlags_ScrollY |
                ImGuiTableFlags_SizingStretchProp;
            if (ImGui::BeginTable("ContentBrowserList", 5, flags))
            {
                ImGui::TableSetupScrollFreeze(0, 1);
                ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.48f);
                ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90.0f);
                ImGui::TableSetupColumn("Origin", ImGuiTableColumnFlags_WidthFixed, 75.0f);
                ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 75.0f);
                ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_WidthFixed, 85.0f);
                ImGui::TableHeadersRow();

                for (const auto& item : items)
                {
                    ImGui::PushID(item.path.c_str());
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    const bool selected = !item.folder && selectedAsset_ == item.path;
                    const std::string label = item.folder ? "[DIR] " + item.name : item.name;
                    ImGui::Selectable(label.c_str(), selected,
                        ImGuiSelectableFlags_AllowDoubleClick |
                        ImGuiSelectableFlags_SpanAllColumns);

                    if (!item.folder && ImGui::IsItemClicked())
                        selectedAsset_ = item.path;
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("%s", item.path.c_str());
                    if (ImGui::IsItemHovered() &&
                        ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                    {
                        if (item.folder)
                            browserFolder_ = item.path;
                        else
                        {
                            selectedAsset_ = item.path;
                            if (item.asset && CanDragAsCookedModel(*item.asset))
                                cookedModelActivation_ = item.path;
                            else if (const auto pair = FindPair(item.path, assets))
                            {
                                const std::string counterpart =
                                    item.path == pair->source ? pair->cooked : pair->source;
                                if (counterpart.starts_with("Assets://"))
                                    revealAsset_ = counterpart;
                            }
                        }
                    }

                    if (!item.folder && item.asset)
                        DrawModelDragSource(*item.asset);
                    if (DrawItemContext(engine, item, assets, browserFolder_,
                                        selectedAsset_, revealAsset_))
                        RefreshAssets(engine);

                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextUnformatted(item.folder ? "Folder" : KindName(item.asset->kind));
                    ImGui::TableSetColumnIndex(2);
                    ImGui::TextUnformatted(item.folder ? "-" : OriginName(item.asset->origin));
                    ImGui::TableSetColumnIndex(3);
                    ImGui::TextUnformatted(item.folder ? "-" : StateName(item.asset->entry.state));
                    ImGui::TableSetColumnIndex(4);
                    if (item.folder)
                        ImGui::TextUnformatted("-");
                    else
                        ImGui::TextUnformatted(HumanBytes(item.asset->entry.bytes).c_str());

                    if (revealNow && !item.folder && selectedAsset_ == item.path)
                        ImGui::SetScrollHereY(0.5f);
                    ImGui::PopID();
                }

                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();

    DrawDetails(assets, selectedAsset_, revealAsset_);
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
