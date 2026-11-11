#include <survival/game_session.hpp>

#include <array>
#include <cstddef>
#include <string_view>
#include <utility>

namespace survival {
namespace {

constexpr int kMapWidth = 10;
constexpr int kMapHeight = 8;
constexpr std::array<std::string_view, kMapHeight> kMap{
    "..........",
    "..#.......",
    "..#..##...",
    "....#.....",
    ".##.#..#..",
    "....#..#..",
    ".......#..",
    "..........",
};

} // namespace

GameSession::GameSession(std::uint32_t seed) {
    Reset(seed);
}

void GameSession::Reset(std::uint32_t seed) {
    seed_ = seed;
    tick_ = 0;
    player_ = {{0, 0}, false};
    message_ = "Find the exit.";
}

GameSnapshot GameSession::Step(Action action) {
    if (player_.arrived) {
        message_ = "The survivor has already reached the exit.";
        return Snapshot();
    }

    Position next = player_.position;
    switch (action) {
    case Action::MoveNorth: --next.y; break;
    case Action::MoveEast:  ++next.x; break;
    case Action::MoveSouth: ++next.y; break;
    case Action::MoveWest:  --next.x; break;
    case Action::Wait:      break;
    }

    ++tick_;
    if (next.x < 0 || next.x >= kMapWidth || next.y < 0 || next.y >= kMapHeight) {
        message_ = "The map boundary blocks that move.";
    } else if (IsBlocked(next)) {
        message_ = "An obstacle blocks that move.";
    } else {
        player_.position = next;
        player_.arrived = next.x == goal_.x && next.y == goal_.y;
        message_ = player_.arrived ? "Exit reached." : "Action applied.";
    }

    return Snapshot();
}

GameSnapshot GameSession::Snapshot() const {
    std::vector<std::string> rows;
    rows.reserve(kMap.size());
    for (const auto row : kMap)
        rows.emplace_back(row);
    return {seed_, tick_, player_, goal_, std::move(rows), message_};
}

bool GameSession::IsBlocked(Position position) const {
    return kMap[static_cast<std::size_t>(position.y)][static_cast<std::size_t>(position.x)] == '#';
}

} // namespace survival
