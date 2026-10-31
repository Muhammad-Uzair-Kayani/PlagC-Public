#include "pgcpch.h"
#include "EditorDataPanels.h"
#include "EditorDragDrop.h"
#include "ImGuiRendererBridge.h"
#include <PlagC/Application.h>
#include <PlagC/Assets/AssetImport.h>
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>

namespace PlagC::Editor {
namespace {

struct PreviewRequest {
    std::string key;
    std::filesystem::path source;
    ThumbnailKind kind = ThumbnailKind::Model;
    std::uint64_t generation = 0;
};

std::string NormalizePreviewPath(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    while (path.starts_with("./")) path.erase(0,2);
    if (path.starts_with("Assets/"))
        return "Assets://" + path.substr(std::string_view("Assets/").size());
    if (path.starts_with("Cooked/"))
        return "Assets://Cooked/" + path.substr(std::string_view("Cooked/").size());
    return path;
}

std::filesystem::path PreviewDiskPath(std::string path)
{
    path = NormalizePreviewPath(std::move(path));
    constexpr std::string_view prefix = "Assets://";
    if (path.starts_with(prefix))
        return std::filesystem::path("Assets") / path.substr(prefix.size());
    return std::filesystem::path(path);
}

std::string LowerPreview(std::string value)
{
    std::transform(value.begin(),value.end(),value.begin(),
                   [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return value;
}

std::optional<ThumbnailKind> PreviewKind(const AssetBrowserEntry& entry)
{
    if (entry.runtimeType)
    {
        if (*entry.runtimeType==AssetType::Model) return ThumbnailKind::Model;
        if (*entry.runtimeType==AssetType::Texture) return ThumbnailKind::Texture;
    }
    const std::string extension=LowerPreview(std::filesystem::path(entry.path).extension().string());
    if (extension==".pgmaterial") return ThumbnailKind::Material;
    switch (entry.kind)
    {
    case EditorAssetKind::ModelSource: return ThumbnailKind::Model;
    case EditorAssetKind::Texture: return ThumbnailKind::Texture;
    default: break;
    }
    if (extension==".plasset")
    {
        try
        {
            const auto disk=PreviewDiskPath(entry.path);
            if (std::filesystem::is_regular_file(ImportReceiptPath(disk)))
            {
                const auto receipt=LoadAssetImportReceipt(disk);
                if (receipt.kind==AssetImportKind::Model) return ThumbnailKind::Model;
                if (receipt.kind==AssetImportKind::Texture) return ThumbnailKind::Texture;
            }
        }
        catch (...) {}
    }
    return std::nullopt;
}

const AssetBrowserEntry* FindPreviewEntry(
    const std::shared_ptr<const std::vector<AssetBrowserEntry>>& snapshot,
    std::string_view selected)
{
    if (!snapshot) return nullptr;
    const std::string wanted=NormalizePreviewPath(std::string(selected));
    for (const auto& entry:*snapshot)
        if (NormalizePreviewPath(entry.path)==wanted)
            return &entry;
    return nullptr;
}

std::optional<PreviewRequest> MakePreviewRequest(const AssetBrowserEntry& entry)
{
    const auto kind=PreviewKind(entry);
    if (!kind) return std::nullopt;
    PreviewRequest request;
    request.key=NormalizePreviewPath(entry.path);
    request.source=PreviewDiskPath(entry.path);
    request.kind=*kind;
    request.generation=entry.generation;
    return request;
}

const char* PreviewStateLabel(ThumbnailState state)
{
    switch (state)
    {
    case ThumbnailState::Queued: return "Queued";
    case ThumbnailState::Generating: return "Generating";
    case ThumbnailState::Ready: return "Ready";
    case ThumbnailState::Failed: return "Failed";
    }
    return "?";
}

void DrawPlaceholder(ImVec2 size, const char* label)
{
    const ImVec2 top=ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(label,size);
    const ImVec2 bottom{top.x+size.x,top.y+size.y};
    ImDrawList* draw=ImGui::GetWindowDrawList();
    draw->AddRectFilled(top,bottom,IM_COL32(28,32,40,255),5.0f);
    const float tile=12.0f;
    for (float y=top.y;y<bottom.y;y+=tile)
        for (float x=top.x;x<bottom.x;x+=tile)
            if ((static_cast<int>((x-top.x)/tile)+static_cast<int>((y-top.y)/tile))&1)
                draw->AddRectFilled({x,y},{std::min(x+tile,bottom.x),std::min(y+tile,bottom.y)},
                                    IM_COL32(35,40,50,255));
    const ImVec2 textSize=ImGui::CalcTextSize(label);
    draw->AddText({top.x+(size.x-textSize.x)*0.5f,top.y+(size.y-textSize.y)*0.5f},
                  IM_COL32(155,165,185,255),label);
    draw->AddRect(top,bottom,IM_COL32(65,74,92,255),5.0f);
}

ThumbnailView DrawPreviewImage(Engine& engine, EditorThumbnailCache& cache,
                               const PreviewRequest& request, ImVec2 size,
                               const char* placeholderId)
{
    const ThumbnailView view=cache.Request(engine,request.key,request.source,
                                           request.kind,request.generation);
    if (view.state==ThumbnailState::Ready && view.texture)
    {
        ImGui::Image(static_cast<ImTextureID>(
                         ImGuiRendererBridge::TextureID(view.texture)),size);
    }
    else
    {
        const char* text=view.state==ThumbnailState::Failed ? "ERR" : "...";
        ImGui::PushID(placeholderId);
        DrawPlaceholder(size,text);
        ImGui::PopID();
        if (view.state==ThumbnailState::Failed && !view.error.empty() && ImGui::IsItemHovered())
            ImGui::SetTooltip("%.*s",static_cast<int>(view.error.size()),view.error.data());
    }
    return view;
}

void DrawPreviewDragSource(const PreviewRequest& request)
{
    const char* payloadType = nullptr;
    const char* description = nullptr;
    switch (request.kind)
    {
    case ThumbnailKind::Model:
        payloadType = DragDrop::CookedModel;
        description = "Model / animation source";
        break;
    case ThumbnailKind::Material:
        payloadType = DragDrop::MaterialAsset;
        description = "Material asset";
        break;
    case ThumbnailKind::Texture:
        return;
    }

    if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        return;

    ImGui::SetDragDropPayload(payloadType, request.key.c_str(), request.key.size() + 1u);
    ImGui::Text("%s", description);
    ImGui::TextDisabled("%s", request.key.c_str());
    if (request.kind == ThumbnailKind::Model)
        ImGui::TextDisabled("Drop into Viewport, Model field, or Animator source.");
    else
        ImGui::TextDisabled("Drop onto a Material slot in Inspector.");
    ImGui::EndDragDropSource();
}

bool StripEligible(const AssetBrowserEntry& entry)
{
    if (entry.runtimeType && (*entry.runtimeType==AssetType::Model ||
                              *entry.runtimeType==AssetType::Texture))
        return true;
    const std::string extension=LowerPreview(std::filesystem::path(entry.path).extension().string());
    return extension==".pgmaterial" || entry.kind==EditorAssetKind::Texture;
}

} // namespace

void EditorDataPanels::Shutdown(Engine& engine)
{
    thumbnails_.Shutdown(engine);
}

void EditorDataPanels::DrawAssetPreview(Engine& engine)
{
    thumbnails_.Pump(engine);
    ImGui::SeparatorText("Thumbnail / Preview");

    const auto snapshot=assets_.Snapshot();
    const AssetBrowserEntry* selected=FindPreviewEntry(snapshot,selectedAsset_);
    if (!selected)
    {
        ImGui::TextDisabled("Select a model, material, or texture to generate a preview.");
    }
    else if (const auto request=MakePreviewRequest(*selected))
    {
        if (ImGui::BeginTable("AssetPreviewDetails",2,
                              ImGuiTableFlags_SizingFixedFit|ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Preview",ImGuiTableColumnFlags_WidthFixed,142.0f);
            ImGui::TableSetupColumn("PreviewInfo",ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            const ThumbnailView view=DrawPreviewImage(engine,thumbnails_,*request,{128,128},"SelectedAssetPreview");
            DrawPreviewDragSource(*request);
            ImGui::TableSetColumnIndex(1);
            ImGui::TextWrapped("%s",request->key.c_str());
            ImGui::Text("Status: %s",PreviewStateLabel(view.state));
            ImGui::TextDisabled("128 x 128 studio preview");
            ImGui::TextDisabled("Drag visual assets from the preview into compatible editor targets.");
            if (view.state==ThumbnailState::Failed && !view.error.empty())
                ImGui::TextWrapped("%.*s",static_cast<int>(view.error.size()),view.error.data());
            if (ImGui::Button("Regenerate Preview"))
                thumbnails_.Invalidate(engine,request->key);
            ImGui::SameLine();
            if (ImGui::Button("Regenerate All"))
                thumbnails_.InvalidateAll(engine);
            ImGui::EndTable();
        }
    }
    else
    {
        ImGui::TextDisabled("This asset type uses its normal file badge; no visual thumbnail is required.");
    }

    ImGui::TextDisabled("Cache: %zu previews | Jobs: %zu",
                        thumbnails_.CachedCount(),thumbnails_.ActiveJobs());

    if (snapshot && !snapshot->empty())
    {
        ImGui::Spacing();
        ImGui::TextDisabled("Preview cache strip");
        std::unordered_set<std::string> shown;
        unsigned displayed=0;
        for (const auto& entry:*snapshot)
        {
            if (displayed>=7 || !StripEligible(entry)) continue;
            const auto request=MakePreviewRequest(entry);
            if (!request || !shown.insert(request->key).second || request->key==selectedAsset_) continue;
            if (displayed) ImGui::SameLine();
            ImGui::BeginGroup();
            DrawPreviewImage(engine,thumbnails_,*request,{54,54},request->key.c_str());
            DrawPreviewDragSource(*request);
            std::string leaf=std::filesystem::path(request->key).filename().string();
            if (leaf.size()>10) leaf=leaf.substr(0,9)+"...";
            ImGui::TextDisabled("%s",leaf.c_str());
            ImGui::EndGroup();
            ++displayed;
        }
    }

    // Upload workers that completed during this UI pass without ever blocking the frame.
    thumbnails_.Pump(engine);
}

EditorPreviewDataPanels::~EditorPreviewDataPanels()
{
    if (!engine_) return;
    try { Shutdown(*engine_); } catch (...) {}
    engine_=nullptr;
}

void EditorPreviewDataPanels::Initialize(Engine& engine)
{
    engine_=&engine;
    thumbnails_.Initialize();
    EditorDataPanels::Initialize(engine);
}

void EditorPreviewDataPanels::DrawContentBrowser(Engine& engine)
{
    EditorDataPanels::DrawContentBrowser(engine);
    DrawAssetPreview(engine);
}

} // namespace PlagC::Editor
