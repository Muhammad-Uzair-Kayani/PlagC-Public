#pragma once

#include "../Framework/Game.h"
#include "../Framework/FpsOverlay.h"
#include <vector>

namespace PlagCGame
{
class RendererBenchmarkGame final : public IGame
{
  public:
    std::string_view Id() const override { return "RendererBenchmark"; }
    std::string_view Name() const override { return "Renderer Backend Benchmark"; }
    std::vector<std::string_view> Worlds() const override { return {"DriverStress"}; }
    std::string_view DefaultWorld() const override { return "DriverStress"; }

    void Start(PlagC::Engine &engine, const PlagC::EngineConfig &config,
               std::string_view world) override;
    void Frame(PlagC::Engine &engine, double dt) override;
    void Stop(PlagC::Engine &engine) override;

  private:
    bool started_ = false;
    bool previousFpsLocked_ = true;
    int previousWidth_ = 0, previousHeight_ = 0;
    PlagC::RenderSettings previousSettings_{};
    PlagC::EnvironmentDesc previousEnvironment_{};
    PlagC::CameraDesc camera_{};
    FpsOverlay fpsOverlay_{};
    std::vector<PlagC::MeshHandle> meshes_;
    std::vector<PlagC::MaterialHandle> materials_;
    std::vector<PlagC::ObjectHandle> objects_;
    PlagC::LightHandle light_{};
};
} // namespace PlagCGame
