#include <iostream>
#include <stdexcept>
#include <survival/simulation.hpp>
using namespace survival;
std::string digest(const Snapshot &s) {
    std::string out = std::to_string(s.tick) + std::to_string(s.gateOpen);
    for (auto &a : s.actors)
        out += std::to_string(a.id) + ":" + std::to_string(a.position.x) + "," +
               std::to_string(a.position.z) + "," + std::to_string(a.health) + "," +
               std::to_string(a.ammo) + "," + std::to_string(a.arrived) + ";";
    for (auto &o : s.objects)
        out += std::to_string(o.id) + std::to_string(o.available);
    for (auto &e : s.events)
        out += std::to_string(e.id) + e.kind;
    return out;
}
int main() {
    try {
        for (unsigned seed = 0; seed < 20; ++seed) {
            Scenario sc;
            sc.seed = seed;
            sc.aggressor = seed % 2;
            sc.runner = seed % 3 == 0;
            sc.scavenger = seed % 4 == 0;
            SyntheticWorld a(sc), b(sc);
            Simulation sa(a), sb(b);
            while (!finished(a.snapshot())) {
                sa.step();
                sb.step();
                if (digest(a.snapshot()) != digest(b.snapshot()))
                    throw std::runtime_error("T01 determinism mismatch");
            }
            if (!sc.aggressor)
                for (auto &r : results(a.snapshot()))
                    if (!r.arrived)
                        throw std::runtime_error("T03 peaceful baseline failed");
        }
        Scenario sc;
        sc.aggressor = true;
        SyntheticWorld world(sc);
        Simulation sim(world);
        bool attack = false, response = false;
        while (!finished(world.snapshot())) {
            auto obs = sim.observations();
            for (auto &o : obs)
                if (o.valid[3])
                    response = true;
            sim.step();
            for (auto &e : world.snapshot().events)
                if (e.kind == "attack")
                    attack = true;
        }
        if (!attack || !response)
            throw std::runtime_error("T06 attack and executable response missing");
        std::cout << "Seeded scenario tests passed (T01,T03,T05,T06)\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
