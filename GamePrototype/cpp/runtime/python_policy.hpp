#pragma once

#include <survival/game_session.hpp>

#include <memory>

namespace survival::client {

// Client-side adapter: Python selects an action; the C++ core validates and
// applies it. Python types never enter the core library.
class PythonPolicyPlugin {
public:
    PythonPolicyPlugin();
    ~PythonPolicyPlugin();

    PythonPolicyPlugin(const PythonPolicyPlugin&) = delete;
    PythonPolicyPlugin& operator=(const PythonPolicyPlugin&) = delete;

    [[nodiscard]] Action ChooseAction(const GameSnapshot& observation);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace survival::client
