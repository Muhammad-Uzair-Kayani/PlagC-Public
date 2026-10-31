#include "pgcpch.h"
#include "EditorPreferences.h"
#include "EditorProfilerPanel.h"
#include "EditorRestart.h"
#include <PlagC/Application.h>
#include <PlagC/Editor/EditorSession.h>
#include <PlagC/Renderer/BackendSelection.h>
#include <imgui.h>

namespace PlagC::Editor {

EditorPreferences* EditorPreferences::active_ = nullptr;

void EditorPreferences::Initialize(Engine& engine)
{
    active_ = this;
    const auto active = engine.Graphics().Backend();
    preferred_ = LoadRendererPreference(DefaultRendererPreferencePath(), active);
    if (preferred_ != RendererBackend::OpenGL && preferred_ != RendererBackend::Vulkan)
        preferred_ = active;
}

bool EditorPreferences::ToggleActive() noexcept
{
    if (!active_)
        return false;
    active_->Toggle();
    return true;
}

bool EditorPreferences::Restart(Engine& engine)
{
    std::string error;
    if (!RestartEditorProcess(error))
    {
        engine.Logger().Write(Severity::Error, "EDITOR", error);
        return false;
    }

    engine.Logger().Write(Severity::Info, "EDITOR", "Restarting Editor to apply renderer setting.");
    engine.Stop();
    open_ = false;
    return true;
}

void EditorPreferences::Draw(Engine& engine, EditorSession& session)
{
    // M20 extends the existing Statistics window after EditorLayer has drawn it.
    // This preserves Window-menu/workspace ownership while keeping profiler history isolated.
    DrawEditorProfilerPanel(engine, session);

    if (!open_)
        return;

    ImGui::SetNextWindowSize(ImVec2(460.0f, 300.0f), ImGuiCond_FirstUseEver);
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
                ImGui::Spacing();

                if (!session.Dirty())
                {
                    if (ImGui::Button("Restart Now"))
                        Restart(engine);
                }
                else
                {
                    const bool canSave = session.HasPath() && session.EditingEnabled();
                    ImGui::BeginDisabled(!canSave);
                    if (ImGui::Button("Save & Restart"))
                    {
                        try
                        {
                            session.Save();
                            Restart(engine);
                        }
                        catch (const std::exception& error)
                        {
                            engine.Logger().Write(Severity::Error, "EDITOR", error.what());
                        }
                    }
                    ImGui::EndDisabled();
                    if (!canSave)
                    {
                        if (!session.HasPath())
                            ImGui::TextDisabled("Save As is required before Save & Restart.");
                        else if (session.Playing())
                            ImGui::TextDisabled("Stop Play mode before Save & Restart.");
                        else
                            ImGui::TextDisabled("Finish the active editor gesture before saving.");
                    }

                    if (ImGui::Button("Restart Without Saving"))
                        ImGui::OpenPopup("Discard Changes And Restart?");

                    if (ImGui::BeginPopupModal("Discard Changes And Restart?", nullptr,
                                               ImGuiWindowFlags_AlwaysAutoResize))
                    {
                        ImGui::TextWrapped(
                            "The current scene has unsaved changes. Restarting now will discard them.");
                        ImGui::Spacing();
                        if (ImGui::Button("Discard & Restart"))
                        {
                            if (Restart(engine))
                                ImGui::CloseCurrentPopup();
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Cancel"))
                            ImGui::CloseCurrentPopup();
                        ImGui::EndPopup();
                    }
                }
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
    if (active_ == this)
        active_ = nullptr;
    open_ = false;
    preferred_ = RendererBackend::Auto;
}

} // namespace PlagC::Editor
