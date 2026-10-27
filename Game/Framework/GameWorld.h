#pragma once

#include <PlagC/Application.h>
#include <string_view>

namespace PlagCGame
{
class IGameWorld
{
  public:
    virtual ~IGameWorld() = default;

    virtual std::string_view Name() const = 0;
    virtual void Start(PlagC::Engine &engine) = 0;
    virtual void Frame(PlagC::Engine &engine, double dt) = 0;
    virtual void Stop(PlagC::Engine &engine) = 0;
};
} // namespace PlagCGame
