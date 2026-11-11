#include <algorithm>
#include <random>
#include <stdexcept>
#include <survival/simulation.hpp>
namespace survival {
void SyntheticWorld::reset(const Scenario &s) {
    if (s.maxTicks < 1 || s.maxTicks > 100000)
        throw std::invalid_argument("maxTicks outside 1..100000");
    state_ = {};
    state_.seed = s.seed;
    state_.maxTicks = s.maxTicks;
    state_.map = std::vector<std::string>(13, std::string(19, '.'));
    for (int z = 0; z < 13; ++z)
        for (int x = 0; x < 19; ++x)
            if (z == 0 || z == 12 || x == 0 || x == 18 || (x == 9 && z <= 10 && z != 5))
                state_.map[z][x] = '#';
    std::mt19937 rng(s.seed);
    for (Id id = 1; id <= 5; ++id) {
        Actor a;
        a.id = id;
        a.position = {1 + int(rng() % 3), 0, 1 + int(id) * 2 - 1};
        if (a.position.z > 10)
            a.position.z = 10;
        state_.actors.push_back(a);
    }
    state_.objects = {{101, "weapon", {4, 0, 3}, true}, {102, "ammo", {5, 0, 7}, true},
                      {103, "medkit", {6, 0, 9}, true}, {104, "weapon", {3, 0, 8}, true},
                      {105, "ammo", {6, 0, 4}, true},   {201, "plate", {8, 0, 4}, true},
                      {202, "plate", {8, 0, 6}, true}};
    state_.goal = {17, 0, 2 + int(rng() % 9)};
    std::vector<Vec3> used;
    for (auto &o : state_.objects)
        if (o.type != "plate") {
            Vec3 p;
            do {
                p = {3 + int(rng() % 5), 0, 2 + int(rng() % 9)};
            } while (std::find(used.begin(), used.end(), p) != used.end());
            o.position = p;
            used.push_back(p);
        }
    if (s.aggressor) {
        Actor a;
        a.id = 10;
        a.position = {6, 0, 5};
        a.weapon = true;
        a.ammo = 20;
        a.role = "aggressor";
        state_.actors.push_back(a);
    }
    if (s.runner) {
        Actor a;
        a.id = 11;
        a.position = {7, 0, 2};
        a.role = "runner";
        state_.actors.push_back(a);
    }
    if (s.scavenger) {
        Actor a;
        a.id = 12;
        a.position = {5, 0, 9};
        a.role = "scavenger";
        state_.actors.push_back(a);
    }
}
void SyntheticWorld::step(const std::vector<Command> &commands) { stepGameplay(state_, commands); }
} // namespace survival
