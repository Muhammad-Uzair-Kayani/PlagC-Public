#include "pgcpch.h"
#include "EditorLayer.h"
#include "EditorPreferences.h"
#include "EditorViewportPresenter.h"
#include "ImGuiInputBridge.h"
#include "ImGuiRendererBridge.h"
#include <PlagC/Application.h>
#include <PlagC/Editor/EditorSession.h>
#include <PlagC/Editor/ViewportTarget.h>
#include <PlagC/World/SceneRenderer.h>
#include <imgui.h>
#include <PlagC/Camera/CameraSystem.h>
#include <algorithm>
#include <cmath>

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
        preferences_.Initialize(engine);

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

        ImGuiIO &io = ImGui::GetIO();
        if (auto *window = engine.MainWindow())
        {
            const int width = window->Width(), height = window->Height();
            const int pixelsWide = window->FramebufferWidth(), pixelsHigh = window->FramebufferHeight();
            if (window->Minimized() || width <= 0 || height <= 0 || pixelsWide <= 0 || pixelsHigh <= 0)
            {
                engine.Graphics().SubmitUI({});
                return;
            }
            io.DisplaySize = ImVec2(float(width), float(height));
            io.DisplayFramebufferScale = ImVec2(float(pixelsWide) / width, float(pixelsHigh) / height);
        }
        io.DeltaTime = dt > 0.0 ? static_cast<float>(dt) : (1.0f / 60.0f);

        ImGui::NewFrame();
        if (!io.WantTextInput && io.KeyCtrl && !io.KeyAlt &&
            ImGui::IsKeyPressed(ImGuiKey_Comma, false))
            preferences_.Toggle();

        layer_.Frame(engine, *session_);
        preferences_.Draw(engine);

        const auto& viewport = layer_.Viewport();
        if (viewport.width > 1 && viewport.height > 1)
            viewportTarget_->Resize(
                static_cast<unsigned>(std::clamp(std::lround(viewport.width * io.DisplayFramebufferScale.x), 1L, 8192L)),
                static_cast<unsigned>(std::clamp(std::lround(viewport.height * io.DisplayFramebufferScale.y), 1L, 8192L)));
        UpdateEditorCamera(engine, dt);
        viewportPresenter_.Draw(viewportTarget_->Handle());
        ImGui::Render();
        renderer_.Submit(engine.Graphics(), ImGui::GetDrawData());
    }

    void OnStop(PlagC::Engine &engine) override
    {
        preferences_.Shutdown();
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
    void UpdateEditorCamera(PlagC::Engine& engine, double dt)
    {
        // Navigation belongs to the editor, never to the saved authoring world.
        const auto& viewport = layer_.Viewport();
        const auto& io = ImGui::GetIO();
        const bool blocked = io.WantTextInput ||
            ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        if (session_->Playing())
            return;
        if (viewport.input.AcceptCameraMouse() && viewport.input.AcceptKeyboard() &&
            !blocked && ImGui::IsMouseDown(ImGuiMouseButton_Right))
        {
            cameraLook_.Rotate(io.MouseDelta.x, io.MouseDelta.y);
            const auto forward = cameraLook_.Forward();
            constexpr float radians = 3.14159265358979323846f / 180.0f;
            const PlagC::Vec3 right{-std::sin(cameraLook_.yaw * radians), 0,
                                    std::cos(cameraLook_.yaw * radians)};
            float x = float(ImGui::IsKeyDown(ImGuiKey_D)) - float(ImGui::IsKeyDown(ImGuiKey_A));
            float y = float(ImGui::IsKeyDown(ImGuiKey_E)) - float(ImGui::IsKeyDown(ImGuiKey_Q));
            float z = float(ImGui::IsKeyDown(ImGuiKey_W)) - float(ImGui::IsKeyDown(ImGuiKey_S));
            const float length = std::max(1.0f, std::sqrt(x*x + y*y + z*z));
            const float step = float(std::clamp(dt, 0.0, 0.1)) * (io.KeyShift ? 24.0f : 8.0f) / length;
            editorCamera_.position.x += (right.x*x + forward.x*z) * step;
            editorCamera_.position.y += (y + forward.y*z) * step;
            editorCamera_.position.z += (right.z*x + forward.z*z) * step;
        }
        const auto forward = cameraLook_.Forward();
        const auto p = editorCamera_.position;
        editorCamera_.target = {p.x + forward.x, p.y + forward.y, p.z + forward.z};
        editorCamera_.aspect = viewport.Aspect();
        engine.Graphics().SetCamera(editorCamera_);
    }

    PlagC::CameraDesc editorCamera_{};
    PlagC::CameraLook cameraLook_{-128.6598f, -25.1041f, .12f};
    std::unique_ptr<PlagC::Editor::EditorSession> session_;
    std::unique_ptr<PlagC::SceneRenderer> sceneRenderer_;
    std::unique_ptr<PlagC::Editor::ViewportTarget> viewportTarget_;
    PlagC::Editor::ImGuiInputBridge input_;
    PlagC::Editor::ImGuiRendererBridge renderer_;
    PlagC::Editor::EditorLayer layer_;
    PlagC::Editor::EditorPreferences preferences_;
    PlagC::Editor::EditorViewportPresenter viewportPresenter_;
};
} // namespace

std::unique_ptr<PlagC::Application>
PlagC::CreateApplication(const PlagC::EngineConfig &)
{
    return std::make_unique<EditorApplication>();
}
