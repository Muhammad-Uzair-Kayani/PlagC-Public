#pragma once

#include "Game.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace PlagCGame
{
class GameRegistry
{
  public:
    using Factory = std::function<GamePtr()>;

    void Register(std::string id, std::string name, Factory factory);
    bool Contains(std::string_view id) const;
    GamePtr Create(std::string_view id) const;
    std::vector<std::pair<std::string, std::string>> Entries() const;

  private:
    struct Entry
    {
        std::string name;
        Factory factory;
    };
    std::unordered_map<std::string, Entry> entries_;
};
} // namespace PlagCGame
