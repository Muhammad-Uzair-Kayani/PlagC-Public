#include "pgcpch.h"
#include "EditorLayer.h"
#include "ImGuiInputBridge.h"
#include <PlagC/Application.h>
#include <PlagC/Editor/EditorSession.h>
#include <imgui.h>

namespace
{
class EditorApplication final : public PlagC::Application
{
public:
    void OnStart(PlagC::Engine &engine) override
    {
        if (auto *window = engine.MainWindow())
            window->SetTitle("PlagC Editor");

        engine.SetFPSLocked(false);
        session_ = std::make_unique<PlagC::Editor::EditorSession>(
            std::make_unique<PlagC::Editor::SceneDocument>(
                std::make_unique<PlagC::World>()));

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();
        input_.Initialize(engine.Events());
        layer_.Initialize(engine, *session_);

        engine.Logger().Write(
            PlagC::Severity::Info, "EDITOR",
            "Editor application shell initialized.");
    }

    void OnFrame(PlagC::Engine &engine, double dt) override
    {
        if (!ImGui::GetCurrentContext() || !session_)
            return;

        ImGuiIO &io = ImGui::GetIO();
        if (auto *window = engine.MainWindow())
            io.DisplaySize = ImVec2(static_cast<float>(window->Width()),
                                    static_cast<float>(window->Height()));
        io.DeltaTime = dt > 0.0 ? static_cast<float>(dt) : (1.0f / 60.0f);

        ImGui::NewFrame();
        layer_.Frame(engine, *session_);
        ImGui::Render();
    }

    void OnStop(PlagC::Engine &engine) override
    {
        layer_.Shutdown();
        input_.Shutdown();
        session_.reset();
        if (ImGui::GetCurrentContext())
            ImGui::DestroyContext();
        engine.Logger().Write(
            PlagC::Severity::Info, "EDITOR",
            "Editor application shell shut down.");
    }

private:
    std::unique_ptr<PlagC::Editor::EditorSession> session_;
    PlagC::Editor::ImGuiInputBridge input_;
    PlagC::Editor::EditorLayer layer_;
};
} // namespace

std::unique_ptr<PlagC::Application>
PlagC::CreateApplication(const PlagC::EngineConfig &)
{
    return std::make_unique<EditorApplication>();
}
