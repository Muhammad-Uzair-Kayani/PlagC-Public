#pragma once
#include <PlagC/Application.h>
#include <PlagC/World/SceneRenderer.h>
#include "StressSimulation.h"
#include "StressHud.h"
#include <PlagC/Assets/AssetScene.h>
#include <fstream>
#include <random>
#include <unordered_map>
class StressWorld
{
  public:
    void Start(PlagC::Engine &, const PlagC::EngineConfig &);
    void Frame(PlagC::Engine &, double);
    void Stop(PlagC::Engine &);
    const std::string &AssetStatus() const { return assetStatus_; }

  private:
    PlagC::EngineConfig config_;
    std::unique_ptr<Stress::Simulation> model_;
    std::unique_ptr<PlagC::SceneRenderer> scene_;
    StressHud hud_;
    std::unique_ptr<PlagC::AssetScene> assetScene_;
    PlagC::AssetHandle<PlagC::ModelAsset> beacon_;
    PlagC::AssetHandle<PlagC::AudioClip> chimeClip_;
    std::vector<PlagC::Entity> assetActors_;
    PlagC::AudioSource chime_;
    bool animationPlaying_ = true, assetErrorReported_ = false;
    void UpdateAssets(PlagC::Engine &, double);
    void AssetMessage(std::string);
    std::string assetStatus_ = "ASSETS LOADING";
    struct Mesh
    {
        PlagC::ColliderComponent shape;
        PlagC::MeshHandle handle;
    };
    std::vector<Mesh> meshes_;
    std::vector<PlagC::MaterialHandle> materials_;
    std::unordered_map<std::uint64_t, PlagC::RenderableComponent> bound_;
    PlagC::EventBus::Connection key_, mouse_, focus_;
    bool paused_ = false, step_ = false, help_ = true, performance_ = true, debug_ = false,
         captured_ = false, jump_ = false;
    float yaw_ = -90, pitch_ = 0;
    std::uint64_t revision_ = 0, frame_ = 0, previousRays_ = 0, previousCasts_ = 0,
                  previousOverlaps_ = 0;
    double hudClock_ = 0, debugClock_ = 0, queryMs_ = 0, prepareMs_ = 0;
    unsigned waypoint_ = 0;
    std::ofstream csv_;
    struct Sample
    {
        double frame, prepare, physics, input, output, presentation, sync, render, queries,
            gameplay, events, characters, assetAnimation, assetUpload;
    };
    std::vector<Sample> samples_;
    Sample peaks_{};
    std::uint64_t sampleCount_ = 0, selected_ = 0;
    double captureElapsed_ = 0;
    std::mt19937_64 sampling_{42};
    void Load(PlagC::Engine &);
    void Unload(PlagC::Engine &);
    void BindVisuals(PlagC::Renderer &);
    void Capture(PlagC::Engine &, bool);
    void Key(PlagC::Engine &, const PlagC::Event &);
    void Input(PlagC::Engine &, double);
    void Hud(PlagC::Engine &, double);
    void SampleFrame(PlagC::Engine &, double);
    void FinishCapture();
};
