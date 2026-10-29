#include "pgcpch.h"
#include "EditorLayer.h"
#include "EditorViewportPresenter.h"
#include "ImGuiInputBridge.h"
#include "ImGuiRendererBridge.h"
#include <PlagC/Application.h>
#include <PlagC/Editor/EditorSession.h>
#include <PlagC/Editor/ViewportTarget.h>
#include <PlagC/World/SceneRenderer.h>
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
        sceneRenderer_ = std::make_unique<PlagC::SceneRenderer>(engine.Graphics());
        viewportTarget_ = std::make_unique<PlagC::Editor::ViewportTarget>(engine.Graphics());
        if (auto *window = engine.MainWindow(); window && window->Width() && window->Height())
            viewportTarget_->Resize(window->Width(), window->Height());

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();
        input_.Initialize(engine.Events());
        renderer_.Initialize(engine.Graphics());
        layer_.Initialize(engine, *session_);

        engine.Logger().Write(
            PlagC::Severity::Info, "EDITOR",
            "Editor application shell initialized.");
    }

    void OnFrame(PlagC::Engine &engine, double dt) override
    {
        if (!ImGui::GetCurrentContext() || !session_ || !sceneRenderer_ || !viewportTarget_)
            return;

        sceneRenderer_->Sync(session_->ViewWorld());
        engine.timings.sceneSyncMs = sceneRenderer_->Statistics().totalMs;

        const PlagC::Editor::ViewportState &viewport = layer_.Viewport();
        if (viewport.width > 1 && viewport.height > 1)
            viewportTarget_->Resize(viewport.width, viewport.height);

        ImGuiIO &io = ImGui::GetIO();
        if (auto *window = engine.MainWindow())
            io.DisplaySize = ImVec2(static_cast<float>(window->Width()),
                                    static_cast<float>(window->Height()));
        io.DeltaTime = dt > 0.0 ? static_cast<float>(dt) : (1.0f / 60.0f);

        ImGui::NewFrame();
        layer_.Frame(engine, *session_);
        viewportPresenter_.Draw(viewportTarget_->Handle());
        ImGui::Render();
        renderer_.Submit(engine.Graphics(), ImGui::GetDrawData());
    }

    void OnStop(PlagC::Engine &engine) override
    {
        layer_.Shutdown();
        renderer_.Shutdown(engine.Graphics());
        if (sceneRenderer_)
            sceneRenderer_->Clear();
        viewportTarget_.reset();
        sceneRenderer_.reset();
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
    std::unique_ptr<PlagC::SceneRenderer> sceneRenderer_;
    std::unique_ptr<PlagC::Editor::ViewportTarget> viewportTarget_;
    PlagC::Editor::ImGuiInputBridge input_;
    PlagC::Editor::ImGuiRendererBridge renderer_;
    PlagC::Editor::EditorLayer layer_;
    PlagC::Editor::EditorViewportPresenter viewportPresenter_;
};
} // namespace

std::unique_ptr<PlagC::Application>
PlagC::CreateApplication(const PlagC::EngineConfig &)
{
    return std::make_unique<EditorApplication>();
}
