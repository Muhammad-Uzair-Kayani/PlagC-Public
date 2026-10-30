#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <unordered_map>
#include <variant>
#include <stdexcept>
#include <cmath>
#include <optional>
namespace PlagC
{
enum class AnimationParameterType
{
    Float,
    Bool,
    Trigger
};
enum class AnimationCompare
{
    Greater,
    Less,
    Equal,
    NotEqual
};
struct AnimationParameter
{
    std::string name;
    AnimationParameterType type;
    float initial = 0;
};
struct AnimationCondition
{
    std::string parameter;
    AnimationCompare compare = AnimationCompare::Equal;
    float value = 1;
};
struct AnimationNode
{
    std::string name;
    unsigned clip = 0;
    bool loop = true;
    float speed = 1;
};
struct AnimationTransition
{
    unsigned from = 0, to = 0;
    float crossfade = .2f;
    std::vector<AnimationCondition> conditions;
};
struct AnimationStateMachine
{
    std::vector<AnimationParameter> parameters;
    std::vector<AnimationNode> states;
    std::vector<AnimationTransition> transitions;
    unsigned entry = 0;
    void Validate(unsigned clips) const
    {
        if (states.empty() || entry >= states.size())
            throw std::invalid_argument("invalid animation entry");
        std::unordered_map<std::string, AnimationParameterType> names;
        for (auto &p : parameters)
        {
            if (p.name.empty() || !std::isfinite(p.initial) ||
                !names.emplace(p.name, p.type).second)
                throw std::invalid_argument("invalid animation parameter");
        }
        std::unordered_map<std::string, bool> nodes;
        for (auto &s : states)
            if (s.name.empty() || !nodes.emplace(s.name, true).second || s.clip >= clips ||
                !std::isfinite(s.speed))
                throw std::invalid_argument("invalid animation state");
        for (auto &t : transitions)
        {
            if (t.from >= states.size() || t.to >= states.size() || !std::isfinite(t.crossfade) ||
                t.crossfade < 0)
                throw std::invalid_argument("invalid animation transition");
            for (auto &c : t.conditions)
                if (!names.contains(c.parameter) || !std::isfinite(c.value))
                    throw std::invalid_argument("invalid animation condition");
        }
    }
};
class AnimationController
{
    std::shared_ptr<const AnimationStateMachine> graph_;
    std::vector<float> values_;
    unsigned current_ = 0;
    std::size_t Parameter(const std::string &name, AnimationParameterType type) const
    {
        if (graph_)
            for (std::size_t i = 0; i < graph_->parameters.size(); ++i)
                if (graph_->parameters[i].name == name)
                {
                    if (graph_->parameters[i].type != type)
                        throw std::invalid_argument("animation parameter type mismatch");
                    return i;
                }
        throw std::invalid_argument("unknown animation parameter");
    }
    std::size_t Parameter(std::string_view name) const
    {
        if (graph_)
            for (std::size_t i = 0; i < graph_->parameters.size(); ++i)
                if (graph_->parameters[i].name == name)
                    return i;
        throw std::invalid_argument("unknown animation parameter");
    }

  public:
    explicit AnimationController(const AnimationStateMachine &graph, unsigned clips)
        : graph_(std::make_shared<const AnimationStateMachine>(graph)), current_(graph.entry)
    {
        graph.Validate(clips);
        for (auto &p : graph.parameters)
            values_.push_back(p.initial);
    }
    void SetFloat(const std::string &name, float v)
    {
        if (!std::isfinite(v))
            throw std::invalid_argument("invalid animation parameter");
        values_.at(Parameter(name, AnimationParameterType::Float)) = v;
    }
    void SetBool(const std::string &name, bool v)
    {
        values_.at(Parameter(name, AnimationParameterType::Bool)) = v ? 1.f : 0.f;
    }
    void Trigger(const std::string &name)
    {
        values_.at(Parameter(name, AnimationParameterType::Trigger)) = 1;
    }
    void ResetTrigger(const std::string &name)
    {
        values_.at(Parameter(name, AnimationParameterType::Trigger)) = 0;
    }
    float Value(std::string_view name) const
    {
        return values_.at(Parameter(name));
    }
    unsigned CurrentIndex() const noexcept { return current_; }
    void SetState(unsigned state)
    {
        if (!graph_ || state >= graph_->states.size())
            throw std::invalid_argument("animation state out of range");
        current_ = state;
    }
    const AnimationNode &Current() const
    {
        return graph_->states[current_];
    }
    const AnimationStateMachine &Graph() const
    {
        return *graph_;
    }
    std::optional<AnimationTransition> Evaluate()
    {
        for (const auto &t : graph_->transitions)
            if (t.from == current_)
            {
                bool match = true;
                for (auto &c : t.conditions)
                {
                    float value = 0;
                    for (std::size_t i = 0; i < graph_->parameters.size(); ++i)
                        if (graph_->parameters[i].name == c.parameter)
                            value = values_[i];
                    switch (c.compare)
                    {
                    case AnimationCompare::Greater:
                        match &= value > c.value;
                        break;
                    case AnimationCompare::Less:
                        match &= value < c.value;
                        break;
                    case AnimationCompare::Equal:
                        match &= value == c.value;
                        break;
                    case AnimationCompare::NotEqual:
                        match &= value != c.value;
                        break;
                    }
                }
                if (!match)
                    continue;
                for (auto &c : t.conditions)
                    for (std::size_t i = 0; i < graph_->parameters.size(); ++i)
                        if (graph_->parameters[i].name == c.parameter &&
                            graph_->parameters[i].type == AnimationParameterType::Trigger)
                            values_[i] = 0;
                current_ = t.to;
                return t;
            }
        return {};
    }
};
} // namespace PlagC
