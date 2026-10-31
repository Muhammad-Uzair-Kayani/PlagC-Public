#include "pgcpch.h"
#include "EditorDataPanels.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace PlagC::Editor {
namespace {

struct ConsoleGroup
{
    LogRecord record;
    std::string lastTimestamp;
    std::uint32_t repeats = 1;
};

std::string LowerConsole(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool IsProfilerRecord(const LogRecord& record)
{
    return LowerConsole(record.category) == "profiler";
}

const char* SeverityBadge(Severity severity)
{
    switch (severity)
    {
    case Severity::Trace: return "T";
    case Severity::Debug: return "D";
    case Severity::Info: return "i";
    case Severity::Warning: return "!";
    case Severity::Error: return "X";
    case Severity::Critical: return "!!";
    }
    return "?";
}

const char* SeverityLabel(Severity severity)
{
    switch (severity)
    {
    case Severity::Trace: return "Trace";
    case Severity::Debug: return "Debug";
    case Severity::Info: return "Info";
    case Severity::Warning: return "Warning";
    case Severity::Error: return "Error";
    case Severity::Critical: return "Critical";
    }
    return "?";
}

ImVec4 SeverityColour(Severity severity)
{
    switch (severity)
    {
    case Severity::Trace: return {0.58f, 0.62f, 0.70f, 1.0f};
    case Severity::Debug: return {0.47f, 0.68f, 0.96f, 1.0f};
    case Severity::Info: return {0.62f, 0.82f, 0.66f, 1.0f};
    case Severity::Warning: return {0.95f, 0.76f, 0.34f, 1.0f};
    case Severity::Error: return {0.96f, 0.43f, 0.39f, 1.0f};
    case Severity::Critical: return {1.00f, 0.30f, 0.62f, 1.0f};
    }
    return {1,1,1,1};
}

std::string CompactMessage(std::string_view message)
{
    std::string result;
    result.reserve(std::min<std::size_t>(message.size(), 180));
    bool spacing = false;
    for (char c : message)
    {
        if (c == '\r' || c == '\n' || c == '\t')
        {
            spacing = !result.empty();
            continue;
        }
        if (spacing)
        {
            if (!result.empty() && result.back() != ' ')
                result.push_back(' ');
            spacing = false;
        }
        result.push_back(c);
        if (result.size() == 176 && message.size() > result.size())
        {
            result += "...";
            break;
        }
    }
    if (result.empty())
        result = "(empty message)";
    return result;
}

bool SameRepeat(const ConsoleGroup& group, const LogRecord& record)
{
    return group.record.severity == record.severity &&
           group.record.category == record.category &&
           group.record.thread == record.thread &&
           group.record.threadID == record.threadID &&
           group.record.message == record.message;
}

std::vector<ConsoleGroup> BuildConsoleGroups(std::span<const LogRecord> records,
                                             std::string_view category,
                                             bool profiler,
                                             bool collapseRepeats)
{
    std::vector<ConsoleGroup> result;
    result.reserve(records.size());

    for (const auto& record : records)
    {
        if (IsProfilerRecord(record) != profiler)
            continue;
        if (!profiler && category != "All" && record.category != category)
            continue;

        if (collapseRepeats && !result.empty() && SameRepeat(result.back(), record))
        {
            ++result.back().repeats;
            result.back().lastTimestamp = record.timestamp;
            continue;
        }

        ConsoleGroup group;
        group.record = record;
        group.lastTimestamp = record.timestamp;
        result.push_back(std::move(group));
    }
    return result;
}

std::vector<std::string> BuildCategories(std::span<const LogRecord> records)
{
    std::vector<std::string> categories;
    for (const auto& record : records)
    {
        if (IsProfilerRecord(record) || record.category.empty())
            continue;
        categories.push_back(record.category);
    }
    std::sort(categories.begin(), categories.end());
    categories.erase(std::unique(categories.begin(), categories.end()), categories.end());
    return categories;
}

std::string CopyDetails(const ConsoleGroup& group)
{
    std::string text;
    text.reserve(group.record.message.size() + 160);
    text += group.record.timestamp;
    text += " [";
    text += SeverityLabel(group.record.severity);
    text += "] [";
    text += group.record.category;
    text += "] ";
    text += group.record.message;
    text += "\nThread: ";
    text += group.record.thread;
    if (!group.record.threadID.empty())
    {
        text += " (";
        text += group.record.threadID;
        text += ')';
    }
    if (group.repeats > 1)
    {
        text += "\nRepeated: ";
        text += std::to_string(group.repeats);
        text += " times, last at ";
        text += group.lastTimestamp;
    }
    return text;
}

void DrawConsoleRows(const std::vector<ConsoleGroup>& groups,
                     std::unordered_set<std::uint64_t>& expanded,
                     bool autoScroll)
{
    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_SizingStretchProp;

    if (!ImGui::BeginTable("ConsoleEntries", 4, flags, ImVec2(0.0f, 0.0f)))
        return;

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 34.0f);
    ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 86.0f);
    ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, 120.0f);
    ImGui::TableSetupColumn("Message", ImGuiTableColumnFlags_WidthStretch);
    ImGui::TableHeadersRow();

    const bool atBottom = ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 3.0f;

    for (const auto& group : groups)
    {
        const auto sequence = group.record.sequence;
        const bool isExpanded = expanded.contains(sequence);
        const ImVec4 colour = SeverityColour(group.record.severity);

        char rowId[40]{};
        std::snprintf(rowId, sizeof(rowId), "%llu", static_cast<unsigned long long>(sequence));
        ImGui::PushID(rowId);
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::TextColored(colour, "%s", SeverityBadge(group.record.severity));
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", SeverityLabel(group.record.severity));

        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("%s", group.record.timestamp.c_str());

        ImGui::TableSetColumnIndex(2);
        ImGui::TextColored(colour, "%s", group.record.category.c_str());

        ImGui::TableSetColumnIndex(3);
        std::string message = CompactMessage(group.record.message);
        if (group.repeats > 1)
            message += "  x" + std::to_string(group.repeats);

        if (ImGui::Selectable(message.c_str(), isExpanded,
                              ImGuiSelectableFlags_AllowDoubleClick))
        {
            if (isExpanded)
                expanded.erase(sequence);
            else
                expanded.insert(sequence);
        }

        if (ImGui::BeginPopupContextItem("ConsoleEntryContext"))
        {
            if (ImGui::MenuItem("Copy Message"))
                ImGui::SetClipboardText(group.record.message.c_str());
            if (ImGui::MenuItem("Copy Details"))
            {
                const std::string details = CopyDetails(group);
                ImGui::SetClipboardText(details.c_str());
            }
            if (ImGui::MenuItem("Copy Category"))
                ImGui::SetClipboardText(group.record.category.c_str());
            ImGui::Separator();
            if (ImGui::MenuItem(isExpanded ? "Collapse Details" : "Expand Details"))
            {
                if (isExpanded)
                    expanded.erase(sequence);
                else
                    expanded.insert(sequence);
            }
            ImGui::EndPopup();
        }

        if (isExpanded)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextDisabled("#%llu",
                static_cast<unsigned long long>(group.record.sequence));
            ImGui::TableSetColumnIndex(1);
            ImGui::TextDisabled("%s", group.record.timestamp.c_str());
            if (group.repeats > 1 && group.lastTimestamp != group.record.timestamp)
            {
                ImGui::TextDisabled("to");
                ImGui::TextDisabled("%s", group.lastTimestamp.c_str());
            }

            ImGui::TableSetColumnIndex(2);
            ImGui::TextDisabled("%s", SeverityLabel(group.record.severity));
            ImGui::TextDisabled("%s", group.record.thread.c_str());
            if (!group.record.threadID.empty())
                ImGui::TextDisabled("Thread %s", group.record.threadID.c_str());

            ImGui::TableSetColumnIndex(3);
            ImGui::TextWrapped("%s", group.record.message.c_str());
            if (group.repeats > 1)
                ImGui::TextDisabled("Collapsed %u identical consecutive records.",
                                    group.repeats);
        }

        ImGui::PopID();
    }

    if (autoScroll && atBottom && !groups.empty())
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndTable();
}

} // namespace

void EditorPreviewDataPanels::DrawConsole(Engine& engine)
{
    ImGui::SetNextItemWidth(250.0f);
    if (ImGui::InputTextWithHint("##console-search", "Search category, thread, message...",
                                 consoleSearch_.data(), consoleSearch_.size()))
        consoleFilter_.search = consoleSearch_.data();

    ImGui::SameLine();
    if (ImGui::Button("Clear"))
    {
        engine.Logger().Clear();
        expandedConsole_.clear();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto-scroll", &autoScrollConsole_);
    ImGui::SameLine();
    ImGui::Checkbox("Collapse repeats", &collapseConsoleRepeats_);

    ImGui::Checkbox("Trace", &consoleFilter_.trace); ImGui::SameLine();
    ImGui::Checkbox("Debug", &consoleFilter_.debug); ImGui::SameLine();
    ImGui::Checkbox("Info", &consoleFilter_.info); ImGui::SameLine();
    ImGui::Checkbox("Warn", &consoleFilter_.warning); ImGui::SameLine();
    ImGui::Checkbox("Error", &consoleFilter_.error); ImGui::SameLine();
    ImGui::Checkbox("Critical", &consoleFilter_.critical);

    const auto all = engine.Logger().Snapshot();
    const auto categories = BuildCategories(all);

    bool categoryStillExists = consoleCategory_ == "All";
    for (const auto& category : categories)
        categoryStillExists = categoryStillExists || category == consoleCategory_;
    if (!categoryStillExists)
        consoleCategory_ = "All";

    ImGui::SetNextItemWidth(190.0f);
    if (ImGui::BeginCombo("Category", consoleCategory_.c_str()))
    {
        const bool allSelected = consoleCategory_ == "All";
        if (ImGui::Selectable("All", allSelected))
            consoleCategory_ = "All";
        if (allSelected)
            ImGui::SetItemDefaultFocus();

        for (const auto& category : categories)
        {
            const bool selected = consoleCategory_ == category;
            if (ImGui::Selectable(category.c_str(), selected))
                consoleCategory_ = category;
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%zu records", all.size());
    ImGui::Separator();

    const auto filtered = FilterLogRecords(all, consoleFilter_);
    const auto regular = BuildConsoleGroups(filtered, consoleCategory_, false,
                                            collapseConsoleRepeats_);
    const auto profiler = BuildConsoleGroups(filtered, "All", true,
                                             collapseConsoleRepeats_);

    if (ImGui::BeginTabBar("ConsoleTabs"))
    {
        if (ImGui::BeginTabItem("Logs"))
        {
            if (regular.empty())
                ImGui::TextDisabled("No log records match the current filters.");
            else
                DrawConsoleRows(regular, expandedConsole_, autoScrollConsole_);
            ImGui::EndTabItem();
        }

        char profilerLabel[64]{};
        std::snprintf(profilerLabel, sizeof(profilerLabel), "Profiler (%zu)###ProfilerTab", profiler.size());
        if (ImGui::BeginTabItem(profilerLabel))
        {
            ImGui::TextDisabled(
                "Large PROFILER reports are isolated here so normal diagnostics stay compact.");
            ImGui::Separator();
            if (profiler.empty())
                ImGui::TextDisabled("No profiler reports match the current filters.");
            else
                DrawConsoleRows(profiler, expandedConsole_, autoScrollConsole_);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

} // namespace PlagC::Editor
