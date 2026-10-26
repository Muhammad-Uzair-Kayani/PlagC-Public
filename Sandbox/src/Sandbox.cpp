#include "pgcpch.h"
#include <PlagC/Application.h>
#include "TPSGame.h"
namespace
{
class Sandbox final : public PlagC::Application
{
    PlagC::EngineConfig config_;
    Game::TPSGame game_;

  public:
    explicit Sandbox(const PlagC::EngineConfig &c) : config_(c)
    {
    }
    void OnStart(PlagC::Engine &e) override
    {
        game_.Start(e, config_);
    }
    void OnFrame(PlagC::Engine &e, double dt) override
    {
        game_.Frame(e, dt);
    }
    void OnStop(PlagC::Engine &e) override
    {
        game_.Stop(e);
    }
};
} // namespace
std::unique_ptr<PlagC::Application> PlagC::CreateApplication(const PlagC::EngineConfig &c)
{
    return std::make_unique<Sandbox>(c);
}
