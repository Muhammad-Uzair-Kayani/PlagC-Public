#include "pgcpch.h"
#include "EditorDataPanels.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <string>

namespace PlagC::Editor {
namespace {

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
    return value;
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

void EditorDataPanels::DrawContentBrowser(Engine& engine)
{
    ImGui::SetNextItemWidth(260.0f);
    ImGui::InputTextWithHint("##asset-search", "Search assets...",
                             assetSearch_.data(), assetSearch_.size());
    ImGui::SameLine();
    if (ImGui::Button("Refresh"))
        RefreshAssets(engine);

    ImGui::Separator();
    const auto snapshot = assets_.Snapshot();
    if (!snapshot || snapshot->empty())
    {
        ImGui::TextDisabled("No assets discovered under Assets/.");
        return;
    }

    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY;
    if (!ImGui::BeginTable("AssetTable", 4, flags, ImVec2(0.0f, 0.0f)))
        return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Path", ImGuiTableColumnFlags_WidthStretch, 0.62f);
    ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, 80.0f);
    ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed, 90.0f);
    ImGui::TableHeadersRow();

    for (const auto& asset : *snapshot)
    {
        if (!Matches(asset.path, assetSearch_.data()))
            continue;
        const bool reveal = !revealAsset_.empty() && IsRevealMatch(asset.path, revealAsset_);
        ImGui::TableNextRow();
        if (reveal)
            ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_HeaderHovered));
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(asset.path.c_str());
        if (!asset.error.empty() && ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", asset.error.c_str());
        if (reveal)
        {
            ImGui::SetScrollHereY(0.5f);
            revealAsset_.clear();
        }
        ImGui::TableSetColumnIndex(1);
        ImGui::TextUnformatted(KindName(asset.kind));
        ImGui::TableSetColumnIndex(2);
        ImGui::TextUnformatted(StateName(asset.state));
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%llu", static_cast<unsigned long long>(asset.bytes));
    }

    ImGui::EndTable();
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