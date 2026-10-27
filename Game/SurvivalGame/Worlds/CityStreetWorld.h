#pragma once

#include "../../Framework/GameWorld.h"
#include <PlagC/Assets/AssetScene.h>
#include <PlagC/Physics/PhysicsSystem.h>
#include <PlagC/World/SceneRenderer.h>
#include <memory>

namespace PlagCGame
{
class CityStreetWorld final : public IGameWorld
{
  public:
    explicit CityStreetWorld(const PlagC::EngineConfig &config) : config_(config) {}

    std::string_view Name() const override { return "CityStreet"; }
    void Start(PlagC::Engine &engine) override;
    void Frame(PlagC::Engine &engine, double dt) override;
    void Stop(PlagC::Engine &engine) override;

  private:
    PlagC::EngineConfig config_;
    std::unique_ptr<PlagC::World> world_;
    std::unique_ptr<PlagC::PhysicsSystem> physics_;
    std::unique_ptr<PlagC::AssetScene> assetScene_;
    std::unique_ptr<PlagC::SceneRenderer> sceneRenderer_;

    PlagC::AssetHandle<PlagC::ModelAsset> playerAsset_;
    PlagC::CharacterHandle character_;
    PlagC::Entity environment_;
    PlagC::Entity player_;
    PlagC::Entity jeep_;
    PlagC::Entity camera_;
    PlagC::Entity sun_;

    PlagC::EventBus::Connection key_;
    PlagC::EventBus::Connection focus_;
    PlagC::EventBus::Connection mouse_;

    unsigned requestedClip_ = 0;
    unsigned activeClip_ = ~0u;
    PlagC::Vec3 playerPosition_{0.0f, 0.0f, 4.0f};
    float playerYaw_ = 180.0f;
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 18.0f;
    bool cursorCaptured_ = true;
    bool jumpQueued_ = false;
};
} // namespace PlagCGame
