#include "python_policy.hpp"

#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <stdexcept>
#include <string>

namespace py = pybind11;

namespace survival::client {

struct PythonPolicyPlugin::Impl {
    py::scoped_interpreter interpreter{};
    py::object policyModule;

    Impl() {
        py::module_::import("sys").attr("path").attr("insert")(
            0, SURVIVAL_PYTHON_MODULE_DIR);
        policyModule = py::module_::import("policy");
    }
};

PythonPolicyPlugin::PythonPolicyPlugin() : impl_(std::make_unique<Impl>()) {}
PythonPolicyPlugin::~PythonPolicyPlugin() = default;

Action PythonPolicyPlugin::ChooseAction(const GameSnapshot& state) {
    py::dict observation;
    observation["seed"] = state.seed;
    observation["tick"] = state.tick;
    observation["position"] = py::make_tuple(state.player.position.x, state.player.position.y);
    observation["goal"] = py::make_tuple(state.goal.x, state.goal.y);
    observation["arrived"] = state.player.arrived;
    observation["map"] = state.mapRows;

    const std::string action = impl_->policyModule.attr("choose_action")(observation).cast<std::string>();
    if (action == "north") return Action::MoveNorth;
    if (action == "east")  return Action::MoveEast;
    if (action == "south") return Action::MoveSouth;
    if (action == "west")  return Action::MoveWest;
    if (action == "wait")  return Action::Wait;
    throw std::runtime_error("Python policy returned an unknown action: " + action);
}

} // namespace survival::client
