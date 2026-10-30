#include "pgcpch.h"
#include "EditorPreferences.h"
#include <PlagC/Application.h>
#include <PlagC/Renderer/BackendSelection.h>
#include <imgui.h>

namespace PlagC::Editor {

void EditorPreferences::Initialize(Engine& engine)
{
    const auto active = engine.Graphics().Backend();
    preferred_ = LoadRendererPreference(DefaultRendererPreferencePath(), active);
    if (preferred_ != RendererBackend::OpenGL && preferred_ != RendererBackend::Vulkan)
        preferred_ = active;
}

void EditorPreferences::Draw(Engine& engine)
{
    if (!open_)
        return;

    ImGui::SetNextWindowSize(ImVec2(460.0f, 260.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Preferences", &open_))
    {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("PreferencesTabs"))
    {
        if (ImGui::BeginTabItem("Graphics"))
        {
            const auto active = engine.Graphics().Backend();
            ImGui::Text("Active renderer: %s", BackendName(active));
            ImGui::Spacing();

            const char* preview = BackendName(preferred_);
            if (ImGui::BeginCombo("Renderer", preview))
            {
                const auto available = Renderer::BackendCapabilities();
                for (const auto& option : AvailableRendererBackends(available))
                {
                    if (option.backend == RendererBackend::Null)
                        continue;
                    const bool selected = preferred_ == option.backend;
                    if (ImGui::Selectable(option.displayName.data(), selected))
                    {
                        try
                        {
                            SaveRendererPreference(DefaultRendererPreferencePath(), option.backend);
                            preferred_ = option.backend;
                            engine.Logger().Write(
                                Severity::Info, "EDITOR",
                                std::string("Preferred renderer set to ") + BackendName(option.backend));
                        }
                        catch (const std::exception& error)
                        {
                            engine.Logger().Write(Severity::Error, "EDITOR", error.what());
                        }
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }

            ImGui::Spacing();
            if (RestartRequired(active))
            {
                ImGui::Separator();
                ImGui::TextWrapped(
                    "Renderer changed to %s. Restart the Editor to apply this setting.",
                    BackendName(preferred_));
                ImGui::TextDisabled("The current session remains on %s.", BackendName(active));
            }
            else
            {
                ImGui::TextDisabled("Renderer setting is active.");
            }

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::TextDisabled("Shortcut: Ctrl+, opens or closes Preferences.");
    ImGui::End();
}

void EditorPreferences::Shutdown() noexcept
{
    open_ = false;
    preferred_ = RendererBackend::Auto;
}

} // namespace PlagC::Editor
