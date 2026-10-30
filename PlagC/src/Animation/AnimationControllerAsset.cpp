#include "pgcpch.h"
#include <PlagC/Animation/AnimationControllerAsset.h>
#include <fstream>
#include <iomanip>
#include <stdexcept>
#include <string>

namespace PlagC {
namespace {
constexpr unsigned ControllerAssetVersion = 1;

void RequireTag(std::istream& input, const char* expected)
{
    std::string tag;
    if (!(input >> tag) || tag != expected)
        throw std::runtime_error(std::string("Invalid animation controller asset: expected ") + expected);
}
}

void AnimationControllerAsset::Validate(unsigned clips) const
{
    graph.Validate(clips);
    if (layout.size() != graph.states.size())
        throw std::invalid_argument("animation controller layout/state count mismatch");
    for (const auto& node : layout)
        if (!std::isfinite(node.x) || !std::isfinite(node.y))
            throw std::invalid_argument("invalid animation controller node position");
}

void SaveAnimationControllerAsset(const std::filesystem::path& path,
                                  const AnimationControllerAsset& asset,
                                  unsigned clips)
{
    asset.Validate(clips);
    if (path.empty())
        throw std::invalid_argument("animation controller path is empty");
    if (const auto parent = path.parent_path(); !parent.empty())
        std::filesystem::create_directories(parent);

    const auto temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output)
        throw std::runtime_error("failed to open animation controller asset for writing");

    output << "PGANIMCTRL " << ControllerAssetVersion << '\n';
    output << "ENTRY " << asset.graph.entry << '\n';
    output << "PARAMETERS " << asset.graph.parameters.size() << '\n';
    for (const auto& parameter : asset.graph.parameters)
        output << "P " << std::quoted(parameter.name) << ' '
               << static_cast<unsigned>(parameter.type) << ' ' << parameter.initial << '\n';

    output << "STATES " << asset.graph.states.size() << '\n';
    for (std::size_t i = 0; i < asset.graph.states.size(); ++i)
    {
        const auto& state = asset.graph.states[i];
        const auto& layout = asset.layout[i];
        output << "S " << std::quoted(state.name) << ' ' << state.clip << ' '
               << (state.loop ? 1 : 0) << ' ' << state.speed << ' '
               << layout.x << ' ' << layout.y << '\n';
    }

    output << "TRANSITIONS " << asset.graph.transitions.size() << '\n';
    for (const auto& transition : asset.graph.transitions)
    {
        output << "T " << transition.from << ' ' << transition.to << ' '
               << transition.crossfade << ' ' << transition.conditions.size() << '\n';
        for (const auto& condition : transition.conditions)
            output << "C " << std::quoted(condition.parameter) << ' '
                   << static_cast<unsigned>(condition.compare) << ' ' << condition.value << '\n';
    }
    output << "END\n";
    output.flush();
    if (!output)
        throw std::runtime_error("failed while writing animation controller asset");
    output.close();

    std::error_code error;
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
    if (error)
    {
        std::filesystem::remove(temporary);
        throw std::runtime_error("failed to commit animation controller asset: " + error.message());
    }
}

AnimationControllerAsset LoadAnimationControllerAsset(const std::filesystem::path& path,
                                                      unsigned clips)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("failed to open animation controller asset");

    RequireTag(input, "PGANIMCTRL");
    unsigned version = 0;
    if (!(input >> version) || version != ControllerAssetVersion)
        throw std::runtime_error("unsupported animation controller asset version");

    AnimationControllerAsset asset;
    RequireTag(input, "ENTRY");
    if (!(input >> asset.graph.entry))
        throw std::runtime_error("invalid animation controller entry state");

    RequireTag(input, "PARAMETERS");
    std::size_t parameterCount = 0;
    if (!(input >> parameterCount) || parameterCount > 4096)
        throw std::runtime_error("invalid animation controller parameter count");
    asset.graph.parameters.reserve(parameterCount);
    for (std::size_t i = 0; i < parameterCount; ++i)
    {
        RequireTag(input, "P");
        AnimationParameter parameter;
        unsigned type = 0;
        if (!(input >> std::quoted(parameter.name) >> type >> parameter.initial) || type > 2)
            throw std::runtime_error("invalid animation controller parameter");
        parameter.type = static_cast<AnimationParameterType>(type);
        asset.graph.parameters.push_back(std::move(parameter));
    }

    RequireTag(input, "STATES");
    std::size_t stateCount = 0;
    if (!(input >> stateCount) || stateCount > 65536)
        throw std::runtime_error("invalid animation controller state count");
    asset.graph.states.reserve(stateCount);
    asset.layout.reserve(stateCount);
    for (std::size_t i = 0; i < stateCount; ++i)
    {
        RequireTag(input, "S");
        AnimationNode state;
        AnimationNodeLayout layout;
        int loop = 0;
        if (!(input >> std::quoted(state.name) >> state.clip >> loop >> state.speed >> layout.x >> layout.y) ||
            (loop != 0 && loop != 1))
            throw std::runtime_error("invalid animation controller state");
        state.loop = loop != 0;
        asset.graph.states.push_back(std::move(state));
        asset.layout.push_back(layout);
    }

    RequireTag(input, "TRANSITIONS");
    std::size_t transitionCount = 0;
    if (!(input >> transitionCount) || transitionCount > 131072)
        throw std::runtime_error("invalid animation controller transition count");
    asset.graph.transitions.reserve(transitionCount);
    for (std::size_t i = 0; i < transitionCount; ++i)
    {
        RequireTag(input, "T");
        AnimationTransition transition;
        std::size_t conditionCount = 0;
        if (!(input >> transition.from >> transition.to >> transition.crossfade >> conditionCount) ||
            conditionCount > 4096)
            throw std::runtime_error("invalid animation controller transition");
        transition.conditions.reserve(conditionCount);
        for (std::size_t c = 0; c < conditionCount; ++c)
        {
            RequireTag(input, "C");
            AnimationCondition condition;
            unsigned compare = 0;
            if (!(input >> std::quoted(condition.parameter) >> compare >> condition.value) || compare > 3)
                throw std::runtime_error("invalid animation controller condition");
            condition.compare = static_cast<AnimationCompare>(compare);
            transition.conditions.push_back(std::move(condition));
        }
        asset.graph.transitions.push_back(std::move(transition));
    }
    RequireTag(input, "END");
    asset.Validate(clips);
    return asset;
}

} // namespace PlagC
