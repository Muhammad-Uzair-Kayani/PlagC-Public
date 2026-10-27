#pragma once

#include <PlagC/Application.h>
#include <memory>
#include <string_view>
#include <vector>

namespace PlagCGame
{
class IGame
{
  public:
    virtual ~IGame() = default;

    virtual std::string_view Id() const = 0;
    virtual std::string_view Name() const = 0;
    virtual std::vector<std::string_view> Worlds() const = 0;
    virtual std::string_view DefaultWorld() const = 0;

    virtual void Start(PlagC::Engine &engine, const PlagC::EngineConfig &config,
                       std::string_view world) = 0;
    virtual void Frame(PlagC::Engine &engine, double dt) = 0;
    virtual void Stop(PlagC::Engine &engine) = 0;
};

using GamePtr = std::unique_ptr<IGame>;
} // namespace PlagCGame
