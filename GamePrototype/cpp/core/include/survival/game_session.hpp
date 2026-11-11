#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace survival {

struct Position {
    int x{};
    int y{};
};

enum class Action {
    Wait,
    MoveNorth,
    MoveEast,
    MoveSouth,
    MoveWest,
};

struct PlayerState {
    Position position{};
    bool arrived{};
};

// A plain, backend-neutral value returned to any client that needs game state.
struct GameSnapshot {
    std::uint32_t seed{};
    std::uint32_t tick{};
    PlayerState player{};
    Position goal{};
    std::vector<std::string> mapRows;
    std::string message;
};

// Owns the current simulation state. It has no console, rendering, Python, or
// PlagC dependency; clients submit actions and consume snapshots.
class GameSession {
public:
    explicit GameSession(std::uint32_t seed = 0);

    void Reset(std::uint32_t seed = 0);
    GameSnapshot Step(Action action);
    [[nodiscard]] GameSnapshot Snapshot() const;

private:
    [[nodiscard]] bool IsBlocked(Position position) const;

    std::uint32_t seed_{};
    std::uint32_t tick_{};
    PlayerState player_{};
    Position goal_{9, 7};
    std::string message_{"Find the exit."};
};

} // namespace survival
