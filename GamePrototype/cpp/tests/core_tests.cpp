#include <cmath>
#include <iostream>
#include <stdexcept>
#include <survival/simulation.hpp>
using namespace survival;
void check(bool ok, const char *message) {
    if (!ok)
        throw std::runtime_error(message);
}
// Deliberately links no SyntheticWorld implementation, console or Python code.
class FakeWorldPort : public IWorldPort {
  public:
    Snapshot s;
    FakeWorldPort() { reset({}); }
    Snapshot snapshot() const override { return s; }
    void reset(const Scenario &sc) override {
        s = {};
        s.seed = sc.seed;
        s.maxTicks = 160;
        s.map = std::vector<std::string>(13, std::string(19, '.'));
        for (int z = 0; z < 13; ++z)
            for (int x = 0; x < 19; ++x)
                if (z == 0 || z == 12 || x == 0 || x == 18 || (x == 9 && z <= 10 && z != 5))
                    s.map[z][x] = '#';
        for (Id id = 1; id <= 5; ++id) {
            Actor a;
            a.id = id;
            a.position = {2, 0, int(id) * 2};
            s.actors.push_back(a);
        }
        s.objects = {{101, "weapon", {3, 0, 2}, true},
                     {102, "ammo", {4, 0, 2}, true},
                     {201, "plate", {8, 0, 4}, true},
                     {202, "plate", {8, 0, 6}, true}};
    }
    void step(const std::vector<Command> &c) override { stepGameplay(s, c); }
};
void tests() {
    FakeWorldPort w;
    auto start = w.s.actors[0].position;
    auto astar = navigate(w.s, start, w.s.goal);
    auto bfs = navigate(w.s, start, w.s.goal, "bfs");
    check(!astar.empty() && astar.size() == bfs.size(), "T02 A* shortest solo bypass");
    for (auto alg : {"dfs", "greedy"})
        check(!navigate(w.s, start, w.s.goal, alg).empty(), "alternate search");
    auto trapped = w.s;
    trapped.map[1][2] = '#';
    trapped.map[2][1] = '#';
    trapped.map[2][3] = '#';
    trapped.map[3][2] = '#';
    check(navigate(trapped, start, trapped.goal).empty(), "T08 unreachable");
    w.s.map[2][3] = '#';
    auto alternative = navigate(w.s, start, w.s.goal);
    check(!alternative.empty() && alternative.front() != Vec3{3, 0, 2},
          "T08 blocked edge replanning");
    w.reset({});
    Simulation sim(w);
    while (!finished(w.s))
        sim.step();
    for (auto &r : results(w.s))
        check(r.arrived && r.alive, "T03 five peaceful arrivals");
    w.reset({});
    w.s.actors[0].position = {8, 0, 4};
    w.s.actors[1].position = {8, 0, 6};
    w.step({});
    w.step({});
    check(!w.s.gateOpen, "T04 proximity not interaction");
    w.step({Command{1, {}, true, false, false, false, false, false, 201}});
    w.step({Command{1, {}, true, false, false, false, false, false, 202}});
    check(!w.s.gateOpen, "T04 same actor cannot open");
    std::vector<Command> pair{{1, {}, true, false, false, false, false, false, 201},
                              {2, {}, true, false, false, false, false, false, 202}};
    w.step(pair);
    check(!w.s.gateOpen, "T04 hold duration");
    w.step({});
    check(w.s.gateHold == 0, "T04 hold resets");
    w.step(pair);
    w.step(pair);
    check(w.s.gateOpen, "T04 actual distinct interactions open gate");
    w.reset({});
    Memory seen, unseen;
    w.s.actors[0].position = {8, 0, 5};
    w.s.actors[1].position = {15, 0, 5};
    w.s.events = {{1, 0, 3, 1, "help", {8, 0, 4}, 1}};
    observe(w.s, 1, seen);
    observe(w.s, 2, unseen);
    check(seen.beliefs[3].trust > 0 && unseen.beliefs[3].trust == 0,
          "T07 unseen help cannot affect trust");
    float trust = seen.beliefs[3].trust;
    observe(w.s, 1, seen);
    check(seen.beliefs[3].trust == trust, "event exactly once");
    w.reset({});
    w.s.actors[0].position = {8, 0, 5};
    w.s.objects.push_back({999, "weapon", {10, 0, 5}, true});
    Memory m;
    auto o = observe(w.s, 1, m);
    for (auto id : o.visibleObjects)
        check(id != 999, "hidden loot leak");
    for (auto f : o.features)
        check(std::isfinite(f) && f >= 0 && f <= 1, "normalized feature");
    w.reset({});
    w.step({{1, {50, 0, 0}}});
    check(w.s.actors[0].position == start, "invalid move");
    w.step({{1, {1, 0, 0}}, {1, {0, 0, 1}}});
    check(w.s.actors[0].position == start, "duplicate actor commands rejected");
    w.step({Command{1, {}, true}});
    check(w.s.actors[0].weapon && w.s.actors[0].pickups == 1, "pickup works");
    w.step({Command{1, {}, true}});
    check(w.s.actors[0].pickups == 1, "T10 no duplicate loot");
    w.s.actors[1].position = {5, 0, 2};
    w.s.actors[0].ammo = 0;
    w.step({Command{1, {}, false, true, false, false, false, false, 2}});
    check(w.s.actors[1].health == 100, "no ammo no damage");
    w.s.actors[0].ammo = 4;
    for (int k = 0; k < 4; ++k)
        w.step({Command{1, {}, false, true, false, false, false, false, 2}});
    check(!w.s.actors[1].alive && w.s.actors[0].ammo == 0, "combat death and ammo");
    auto idle = results(w.s)[0].fitness;
    w.s.actors[0].arrived = true;
    w.s.actors[0].arrivalTick = w.s.tick;
    check(results(w.s)[0].fitness > idle + 900, "T10 completion dominates idle and harm");
    w.reset({});
    w.s.tick = 160;
    auto before = w.s.tick;
    w.step({{1, {1, 0, 0}}});
    check(w.s.tick == before && finished(w.s), "timeout immutable");
    std::cout << "Core/FakeWorldPort tests passed (T02-T08,T10,T12)\n";
}
int main() {
    try {
        tests();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
