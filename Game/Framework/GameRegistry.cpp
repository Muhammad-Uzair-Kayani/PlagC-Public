#include "GameRegistry.h"
#include <stdexcept>

namespace PlagCGame
{
void GameRegistry::Register(std::string id, std::string name, Factory factory)
{
    if (id.empty() || !factory)
        throw std::invalid_argument("invalid game registration");
    auto [it, inserted] = entries_.emplace(std::move(id), Entry{std::move(name), std::move(factory)});
    if (!inserted)
        throw std::runtime_error("duplicate game id: " + it->first);
}

bool GameRegistry::Contains(std::string_view id) const
{
    return entries_.find(std::string(id)) != entries_.end();
}

GamePtr GameRegistry::Create(std::string_view id) const
{
    auto it = entries_.find(std::string(id));
    if (it == entries_.end())
        return {};
    return it->second.factory();
}

std::vector<std::pair<std::string, std::string>> GameRegistry::Entries() const
{
    std::vector<std::pair<std::string, std::string>> result;
    result.reserve(entries_.size());
    for (const auto &[id, entry] : entries_)
        result.emplace_back(id, entry.name);
    return result;
}
} // namespace PlagCGame
