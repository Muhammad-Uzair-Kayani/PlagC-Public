#include "pgcpch.h"
#include "EditorLayer.h"
#include "EditorPlayRuntime.h"
#include "EditorPreferences.h"
#include "EditorViewportPresenter.h"
#include "ImGuiInputBridge.h"
#include "ImGuiRendererBridge.h"
#include <PlagC/Application.h>
#include <PlagC/Assets/AssetScene.h>
#include <PlagC/Editor/EditorSession.h>
#include <PlagC/Editor/ScenePicking.h>
#include <PlagC/Editor/ViewportTarget.h>
#include <PlagC/World/SceneRenderer.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <PlagC/Camera/CameraSystem.h>
#include <algorithm>
#include <array>
#include <cmath>

namespace
{
class EditorApplication final : public PlagC::Application
{
public:
    explicit EditorApplication(const PlagC::EngineConfig& config) : playRuntime_(config) {}

    void OnStart(PlagC::Engine &engine) override
    {
        if (auto *window = engine.MainWindow()) window->SetTitle("PlagC Editor");
        engine.SetFPSLocked(false);
        session_ = std::make_unique<PlagC::Editor::EditorSession>(
            std::make_unique<PlagC::Editor::SceneDocument>(std::make_unique<PlagC::World>()));
        sceneRenderer_ = std::make_unique<PlagC::SceneRenderer>(engine.Graphics());
        assetScene_ = std::make_unique<PlagC::AssetScene>(engine.Graphics());
        viewportTarget_ = std::make_unique<PlagC::Editor::ViewportTarget>(engine.Graphics());
        if (auto *window = engine.MainWindow(); window && window->Width() && window->Height())
            viewportTarget_->Resize(window->Width(), window->Height());

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
        ImGui::StyleColorsDark();
        input_.Initialize(engine.Events());
        playRuntime_.Initialize(engine);
        renderer_.Initialize(engine.Graphics());
        layer_.Initialize(engine, *session_);
        preferences_.Initialize(engine);

        editorPreviousEnvironment_ = engine.Graphics().Environment();
        editorPreviousBackground_ = engine.Graphics().Settings().background;
        CreateEditorPreviewLights(engine);
        UpdateEditorPreviewEnvironment(engine, true);

        closeConnection_ = engine.Events().Subscribe(PlagC::EventType::Close,
            [this, &engine](const PlagC::Event&) {
                if (!session_) return;
                if (!session_->Dirty() && !session_->Document().HasTransientEdit()) return;
                if (auto* window = engine.MainWindow()) window->CancelClose();
                closePrompt_ = true;
            });
        engine.Logger().Write(PlagC::Severity::Info, "EDITOR", "Editor application shell initialized.");
    }

    void OnFrame(PlagC::Engine &engine, double dt) override
    {
        if (!ImGui::GetCurrentContext() || !session_ || !sceneRenderer_ || !assetScene_ || !viewportTarget_)
            return;

        std::span<const PlagC::TransformUpdate> presentation;
        try
        {
            presentation = playRuntime_.Update(engine, *session_, dt);
        }
        catch (const std::exception& error)
        {
            engine.Logger().Write(PlagC::Severity::Error, "EDITOR-PLAY", error.what());
            playRuntime_.Reset(engine);
            if (session_->Playing()) session_->StopPlay();
            engine.timings.physicsMs = 0.0;
            engine.timings.audioMs = 0.0;
        }

        sceneRenderer_->Sync(session_->ViewWorld(), presentation);
        engine.timings.sceneSyncMs = sceneRenderer_->Statistics().totalMs;
        const PlagC::Entity animationPreview = session_->Playing()
            ? PlagC::Entity{}
            : layer_.AnimatorPreviewEntity(*session_);
        const double animationDelta = session_->Playing()
            ? playRuntime_.SimulationDelta()
            : (animationPreview ? dt : 0.0);
        assetScene_->Update(session_->SystemWorld(), animationDelta, animationPreview, !session_->Playing());
        engine.timings.animationMs = assetScene_->Statistics().animationMs;
        try
        {
            // Milestone 18 ordering: gameplay/physics in Update -> animation above -> late gameplay/audio here.
            playRuntime_.CompleteFrame(engine, *session_);
        }
        catch (const std::exception& error)
        {
            engine.Logger().Write(PlagC::Severity::Error, "EDITOR-PLAY", error.what());
            playRuntime_.Reset(engine);
            if (session_->Playing()) session_->StopPlay();
            engine.timings.audioMs = 0.0;
        }

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
        if (!io.WantTextInput && io.KeyCtrl && !io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_Comma, false))
            preferences_.Toggle();

        layer_.Frame(engine, *session_);
        layer_.AppendAssetInteractions(engine, *session_);
        if (layer_.InspectorVisible())
        {
            layer_.AppendComponentInspector(engine, *session_);
            layer_.AppendAnimatorInspector(engine, *session_);
        }

        const auto& viewport = layer_.Viewport();
        const bool popupBlocksGame = ImGui::IsPopupOpen(nullptr,
            ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        playRuntime_.SetGameInputRouting(
            session_->Playing() && !session_->Paused() && viewport.input.viewportFocused &&
                !io.WantTextInput && !popupBlocksGame,
            session_->Playing() && !session_->Paused() && viewport.input.viewportHovered &&
                !viewport.input.gizmoActive && !popupBlocksGame);

        if (viewport.width > 1 && viewport.height > 1)
            viewportTarget_->Resize(
                static_cast<unsigned>(std::clamp(std::lround(viewport.width * io.DisplayFramebufferScale.x), 1L, 8192L)),
                static_cast<unsigned>(std::clamp(std::lround(viewport.height * io.DisplayFramebufferScale.y), 1L, 8192L)));
        UpdateEditorCamera(engine, dt);
        viewportPresenter_.Draw(viewportTarget_->Handle());
        layer_.AppendViewportExperience(engine, *session_, editorCamera_);
        if (session_->Playing())
            engine.Graphics().SetCameraProjection(PlagC::CameraProjection::Perspective);
        UpdateEditorPreviewLights(engine);
        UpdateEditorPreviewEnvironment(engine);
        layer_.AppendViewportAssetDrop(engine, *session_, editorCamera_);
        layer_.AppendTransformGizmo(engine, *session_, editorCamera_);
        HandleViewportPicking(engine);

        preferences_.Draw(engine, *session_);
        layer_.DrawModals(engine, *session_);
        DrawClosePrompt(engine);

        ImGui::Render();
        renderer_.Submit(engine.Graphics(), ImGui::GetDrawData());
    }

    void OnStop(PlagC::Engine &engine) override
    {
        closeConnection_.Reset();
        closePrompt_ = false;
        playRuntime_.Shutdown(engine);
        RestoreEditorPreviewEnvironment(engine);
        DestroyEditorPreviewLights(engine);
        preferences_.Shutdown();
        layer_.Shutdown();
        renderer_.Shutdown(engine.Graphics());
        if (assetScene_) assetScene_->Clear();
        if (sceneRenderer_) sceneRenderer_->Clear();
        viewportTarget_.reset();
        assetScene_.reset();
        sceneRenderer_.reset();
        input_.Shutdown();
        session_.reset();
        if (ImGui::GetCurrentContext()) ImGui::DestroyContext();
        engine.Logger().Write(PlagC::Severity::Info, "EDITOR", "Editor application shell shut down.");
    }

private:
    void CreateEditorPreviewLights(PlagC::Engine& engine)
    {
        editorPreviewLightDescs_[0] = {PlagC::LightType::Directional, {}, {-0.45f,-1.0f,-0.35f}, {1.0f,0.92f,0.82f}, 2.6f, 100.0f, 25.0f, 45.0f};
        editorPreviewLightDescs_[1] = {PlagC::LightType::Directional, {}, {0.70f,-0.55f,0.20f}, {0.45f,0.58f,1.0f}, 1.25f, 100.0f, 25.0f, 45.0f};
        editorPreviewLightDescs_[2] = {PlagC::LightType::Directional, {}, {0.05f,-0.30f,-1.0f}, {1.0f,0.42f,0.25f}, 1.10f, 100.0f, 25.0f, 45.0f};
        try
        {
            for (std::size_t i=0;i<editorPreviewLights_.size();++i)
                editorPreviewLights_[i]=engine.Graphics().CreateLight(editorPreviewLightDescs_[i]);
            editorPreviewLightsEnabled_=true;
        }
        catch (...)
        {
            DestroyEditorPreviewLights(engine);
            throw;
        }
    }

    void UpdateEditorPreviewLights(PlagC::Engine& engine)
    {
        const bool enabled = !session_->Playing() && layer_.Viewport().previewLighting;
        if (enabled == editorPreviewLightsEnabled_) return;
        for (std::size_t i=0;i<editorPreviewLights_.size();++i)
        {
            if (!editorPreviewLights_[i]) continue;
            auto desc=editorPreviewLightDescs_[i];
            if (!enabled) desc.power=0.0f;
            engine.Graphics().UpdateLight(editorPreviewLights_[i],desc);
        }
        editorPreviewLightsEnabled_=enabled;
    }

    void DestroyEditorPreviewLights(PlagC::Engine& engine) noexcept
    {
        for (auto& light : editorPreviewLights_)
        {
            if (!light) continue;
            try { engine.Graphics().DestroyLight(light); } catch (...) {}
            light={};
        }
        editorPreviewLightsEnabled_=false;
    }

    void UpdateEditorPreviewEnvironment(PlagC::Engine& engine, bool force = false)
    {
        const bool enabled = !session_->Playing() && layer_.Viewport().previewLighting;
        if (!force && enabled == editorPreviewEnvironmentEnabled_) return;

        if (enabled)
        {
            // Editor-only sky/reflection environment: this makes PBS assets readable even in
            // scenes that intentionally contain no authored lights or environment yet.
            engine.Graphics().SetEnvironment({"PlagCEnvironment.dds", true, true});
            auto settings = engine.Graphics().Settings();
            settings.background = {0.055f, 0.070f, 0.095f};
            engine.Graphics().SetSettings(settings);
        }
        else
        {
            engine.Graphics().SetEnvironment(editorPreviousEnvironment_);
            auto settings = engine.Graphics().Settings();
            settings.background = editorPreviousBackground_;
            engine.Graphics().SetSettings(settings);
        }
        editorPreviewEnvironmentEnabled_ = enabled;
    }

    void RestoreEditorPreviewEnvironment(PlagC::Engine& engine) noexcept
    {
        try
        {
            engine.Graphics().SetEnvironment(editorPreviousEnvironment_);
            auto settings = engine.Graphics().Settings();
            settings.background = editorPreviousBackground_;
            engine.Graphics().SetSettings(settings);
        }
        catch (...) {}
        editorPreviewEnvironmentEnabled_ = false;
    }

    void HandleViewportPicking(PlagC::Engine& engine)
    {
        const auto& viewport = layer_.Viewport();
        if (!session_->EditingEnabled() || !viewport.input.AcceptSelectionMouse() ||
            !ImGui::IsMouseClicked(ImGuiMouseButton_Left, false) || ImGui::GetDragDropPayload())
            return;
        ImGuiWindow* window = ImGui::FindWindowByName("Viewport");
        if (!window || !window->WasActive || window->Collapsed) return;
        const ImRect rect = window->WorkRect;
        const ImVec2 mouse = ImGui::GetIO().MousePos;
        if (rect.GetWidth() <= 1.0f || rect.GetHeight() <= 1.0f || !rect.Contains(mouse)) return;

        PlagC::Editor::PickRequest request;
        request.x = (mouse.x - rect.Min.x) * static_cast<float>(viewport.width) / rect.GetWidth();
        request.y = (mouse.y - rect.Min.y) * static_cast<float>(viewport.height) / rect.GetHeight();
        request.width = viewport.width;
        request.height = viewport.height;
        request.camera = editorCamera_;
        request.orthographic = viewport.orthographic;
        request.orthographicHeight = viewport.orthographicHeight;
        try
        {
            PlagC::Editor::RendererPickingProvider picker(engine.Graphics(), session_->EditDocument(), *sceneRenderer_, *assetScene_);
            auto& document = session_->EditDocument();
            if (const auto hit = picker.Pick(request);
                hit && !layer_.HierarchyEntityLocked(hit->persistentID))
                document.SelectPersistent(hit->persistentID);
            else
                document.Select({});
        }
        catch (const std::exception& error)
        {
            engine.Logger().Write(PlagC::Severity::Error, "EDITOR-PICK", error.what());
        }
    }

    void DrawClosePrompt(PlagC::Engine& engine)
    {
        if (!closePrompt_) return;
        constexpr const char* popup = "Unsaved Changes##WindowClose";
        if (!ImGui::IsPopupOpen(popup)) ImGui::OpenPopup(popup);
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f,0.5f));
        if (!ImGui::BeginPopupModal(popup, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;
        ImGui::TextWrapped("The current scene has unsaved changes. Save before closing the Editor?");
        ImGui::Spacing();
        const bool canSave = session_->EditingEnabled();
        ImGui::BeginDisabled(!canSave);
        if (ImGui::Button("Save & Exit"))
        {
            if (layer_.SaveScene(engine, *session_))
            {
                closePrompt_=false; ImGui::CloseCurrentPopup();
                if (auto* window=engine.MainWindow()) window->RequestClose();
            }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button("Discard & Exit"))
        {
            closePrompt_=false; ImGui::CloseCurrentPopup();
            if (auto* window=engine.MainWindow()) window->RequestClose();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) { closePrompt_=false; ImGui::CloseCurrentPopup(); }
        if (!canSave)
        {
            ImGui::Spacing();
            if (session_->Playing()) ImGui::TextDisabled("Stop Play mode before saving the scene.");
            else ImGui::TextDisabled("Finish the active editor gesture before saving the scene.");
        }
        ImGui::EndPopup();
    }

    void SyncCameraLookFromEditorCamera()
    {
        const float dx=editorCamera_.target.x-editorCamera_.position.x;
        const float dy=editorCamera_.target.y-editorCamera_.position.y;
        const float dz=editorCamera_.target.z-editorCamera_.position.z;
        const float length=std::sqrt(dx*dx+dy*dy+dz*dz);
        if (length <= 1e-5f) return;
        constexpr float degrees=180.0f/3.14159265358979323846f;
        cameraLook_.pitch=std::asin(std::clamp(dy/length,-1.0f,1.0f))*degrees;
        cameraLook_.yaw=std::atan2(dz,dx)*degrees;
    }

    void UpdateEditorCamera(PlagC::Engine& engine, double dt)
    {
        const auto& viewport = layer_.Viewport();
        const auto& io = ImGui::GetIO();
        const bool blocked = io.WantTextInput || ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        editorCamera_.aspect = viewport.Aspect();
        runtimeCamera_.SetViewport(viewport.width, viewport.height);
        if (session_->Playing())
        {
            engine.Graphics().SetCameraProjection(PlagC::CameraProjection::Perspective);
            if (!runtimeCamera_.Sync(session_->SystemWorld(), engine.Graphics())) engine.Graphics().SetCamera(editorCamera_);
            return;
        }

        engine.Graphics().SetCameraProjection(viewport.orthographic ? PlagC::CameraProjection::Orthographic : PlagC::CameraProjection::Perspective,
                                              viewport.orthographicHeight);
        const float focusDx=editorCamera_.target.x-editorCamera_.position.x;
        const float focusDy=editorCamera_.target.y-editorCamera_.position.y;
        const float focusDz=editorCamera_.target.z-editorCamera_.position.z;
        const float focusDistance=std::clamp(std::sqrt(focusDx*focusDx+focusDy*focusDy+focusDz*focusDz),0.1f,100000.0f);
        SyncCameraLookFromEditorCamera();

        const bool cameraMouse=viewport.input.AcceptCameraMouse()&&!blocked;
        const bool cameraKeyboard=viewport.input.AcceptKeyboard()&&!blocked&&!io.KeyCtrl&&!io.KeyAlt&&!io.KeySuper;
        if(cameraMouse&&ImGui::IsMouseDown(ImGuiMouseButton_Right))
            cameraLook_.Rotate(io.MouseDelta.x, io.MouseDelta.y);

        const auto forward = cameraLook_.Forward();
        constexpr float radians = 3.14159265358979323846f / 180.0f;
        const PlagC::Vec3 right{-std::sin(cameraLook_.yaw * radians),0,std::cos(cameraLook_.yaw * radians)};
        if(cameraKeyboard)
        {
            float x=float(ImGui::IsKeyDown(ImGuiKey_D))-float(ImGui::IsKeyDown(ImGuiKey_A));
            float y=float(ImGui::IsKeyDown(ImGuiKey_E))-float(ImGui::IsKeyDown(ImGuiKey_Q));
            float z=float(ImGui::IsKeyDown(ImGuiKey_W))-float(ImGui::IsKeyDown(ImGuiKey_S));
            const float length=std::max(1.0f,std::sqrt(x*x+y*y+z*z));
            const float baseSpeed=std::clamp(viewport.cameraSpeed,0.1f,200.0f);
            const float step=float(std::clamp(dt,0.0,0.1))*baseSpeed*(io.KeyShift?3.0f:1.0f)/length;
            editorCamera_.position.x+=(right.x*x+forward.x*z)*step;
            editorCamera_.position.y+=(y+forward.y*z)*step;
            editorCamera_.position.z+=(right.z*x+forward.z*z)*step;
        }
        const auto p=editorCamera_.position;
        editorCamera_.target={p.x+forward.x*focusDistance,p.y+forward.y*focusDistance,p.z+forward.z*focusDistance};
        engine.Graphics().SetCamera(editorCamera_);
    }

    PlagC::CameraDesc editorCamera_{};
    PlagC::CameraLook cameraLook_{-128.6598f,-25.1041f,.12f};
    PlagC::CameraSystem runtimeCamera_{};
    std::array<PlagC::LightHandle,3> editorPreviewLights_{};
    std::array<PlagC::LightDesc,3> editorPreviewLightDescs_{};
    bool editorPreviewLightsEnabled_=false;
    PlagC::EnvironmentDesc editorPreviousEnvironment_{};
    PlagC::Vec3 editorPreviousBackground_{};
    bool editorPreviewEnvironmentEnabled_=false;
    std::unique_ptr<PlagC::Editor::EditorSession> session_;
    std::unique_ptr<PlagC::SceneRenderer> sceneRenderer_;
    std::unique_ptr<PlagC::AssetScene> assetScene_;
    std::unique_ptr<PlagC::Editor::ViewportTarget> viewportTarget_;
    PlagC::EventBus::Connection closeConnection_;
    bool closePrompt_=false;
    PlagC::Editor::EditorPlayRuntime playRuntime_;
    PlagC::Editor::ImGuiInputBridge input_;
    PlagC::Editor::ImGuiRendererBridge renderer_;
    PlagC::Editor::EditorLayer layer_;
    PlagC::Editor::EditorPreferences preferences_;
    PlagC::Editor::EditorViewportPresenter viewportPresenter_;
};
} // namespace

std::unique_ptr<PlagC::Application> PlagC::CreateApplication(const PlagC::EngineConfig &config)
{
    return std::make_unique<EditorApplication>(config);
}
