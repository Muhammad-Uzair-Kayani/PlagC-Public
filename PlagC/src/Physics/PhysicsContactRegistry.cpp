#include "pgcpch.h"
#include "PhysicsContactRegistry.h"
#include <map>

namespace PlagC {
namespace {
std::mutex& ContactMutex()
{
    static std::mutex mutex;
    return mutex;
}

std::map<std::pair<std::uint64_t, std::uint64_t>, unsigned>& ContactCounts()
{
    static std::map<std::pair<std::uint64_t, std::uint64_t>, unsigned> counts;
    return counts;
}

std::pair<std::uint64_t, std::uint64_t> Normalize(PhysicsContactPair pair)
{
    if (pair.a > pair.b)
        std::swap(pair.a, pair.b);
    return {pair.a, pair.b};
}
} // namespace

void PhysicsContactRegistry::Add(PhysicsContactPair pair) noexcept
{
    try
    {
        if (!pair.a || !pair.b || pair.a == pair.b)
            return;
        std::lock_guard lock(ContactMutex());
        ++ContactCounts()[Normalize(pair)];
    }
    catch (...) {}
}

void PhysicsContactRegistry::Remove(PhysicsContactPair pair) noexcept
{
    try
    {
        if (!pair.a || !pair.b || pair.a == pair.b)
            return;
        std::lock_guard lock(ContactMutex());
        auto& counts = ContactCounts();
        const auto key = Normalize(pair);
        const auto found = counts.find(key);
        if (found == counts.end())
            return;
        if (found->second > 1)
            --found->second;
        else
            counts.erase(found);
    }
    catch (...) {}
}

std::vector<PhysicsContactPair> PhysicsContactRegistry::Snapshot()
{
    std::lock_guard lock(ContactMutex());
    std::vector<PhysicsContactPair> result;
    result.reserve(ContactCounts().size());
    for (const auto& [pair, count] : ContactCounts())
        if (count)
            result.push_back({pair.first, pair.second});
    return result;
}

} // namespace PlagC
