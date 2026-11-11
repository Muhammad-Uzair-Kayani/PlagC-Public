#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
#include <queue>
#include <random>
#include <set>
#include <stdexcept>
#include <survival/simulation.hpp>
namespace survival {
namespace {
const std::array<Vec3, 4> dirs{{{0, 0, -1}, {1, 0, 0}, {0, 0, 1}, {-1, 0, 0}}};
Vec3 plus(Vec3 a, Vec3 b) { return {a.x + b.x, 0, a.z + b.z}; }
const Actor *actor(const Snapshot &s, Id id) {
    for (auto &a : s.actors)
        if (a.id == id)
            return &a;
    return nullptr;
}
const Object *object(const Snapshot &s, Id id) {
    for (auto &o : s.objects)
        if (o.id == id)
            return &o;
    return nullptr;
}
bool active(const Actor &a) { return a.alive && !a.arrived; }
double unit(double v) { return std::clamp(v, 0.0, 1.0); }
} // namespace
int distance(Vec3 a, Vec3 b) { return std::abs(a.x - b.x) + std::abs(a.z - b.z); }
std::string name(Intent i) {
    static const std::array<std::string, 6> n{"Explore", "Acquire", "Cooperate",
                                              "Avoid",   "Defend",  "Progress"};
    auto k = static_cast<unsigned>(i);
    if (k >= n.size())
        throw std::invalid_argument("unknown intent");
    return n[k];
}
Intent parseIntent(const std::string &s) {
    for (int i = 0; i < 6; ++i)
        if (name(static_cast<Intent>(i)) == s)
            return static_cast<Intent>(i);
    throw std::invalid_argument("unknown intent: " + s);
}
bool blocked(const Snapshot &s, Vec3 p) {
    return p.y != 0 || p.z < 0 || p.z >= int(s.map.size()) || p.x < 0 ||
           p.x >= int(s.map[p.z].size()) || s.map[p.z][p.x] == '#' ||
           (!s.gateOpen && p == Vec3{9, 0, 5});
}
bool lineOfSight(const Snapshot &s, Vec3 a, Vec3 b) {
    int dx = std::abs(b.x - a.x), dz = std::abs(b.z - a.z), sx = a.x < b.x ? 1 : -1,
        sz = a.z < b.z ? 1 : -1, e = dx - dz;
    for (;;) {
        if (blocked(s, a))
            return false;
        if (a == b)
            return true;
        int e2 = 2 * e;
        if (e2 > -dz) {
            e -= dz;
            a.x += sx;
        }
        if (e2 < dx) {
            e += dx;
            a.z += sz;
        }
    }
}
std::vector<Vec3> navigate(const Snapshot &s, Vec3 start, Vec3 goal, const std::string &algorithm) {
    if (algorithm != "astar" && algorithm != "bfs" && algorithm != "dfs" && algorithm != "greedy")
        throw std::invalid_argument("unknown search algorithm");
    if (blocked(s, start) || blocked(s, goal) || start == goal)
        return {};
    auto key = [](Vec3 p) { return p.z * 10000 + p.x; };
    struct Node {
        int priority, order, g;
        Vec3 p;
    };
    struct Compare {
        bool operator()(const Node &a, const Node &b) const {
            return a.priority != b.priority ? a.priority > b.priority : a.order > b.order;
        }
    };
    std::priority_queue<Node, std::vector<Node>, Compare> q;
    std::map<int, int> costs;
    std::map<int, Vec3> parent;
    int order = 0;
    costs[key(start)] = 0;
    q.push({0, order++, 0, start});
    while (!q.empty()) {
        auto n = q.top();
        q.pop();
        if (n.g != costs[key(n.p)])
            continue;
        if (n.p == goal) {
            std::vector<Vec3> path;
            for (Vec3 p = goal; !(p == start); p = parent.at(key(p)))
                path.push_back(p);
            std::reverse(path.begin(), path.end());
            return path;
        }
        for (auto d : dirs) {
            auto p = plus(n.p, d);
            if (blocked(s, p))
                continue;
            int g = n.g + 1, k = key(p);
            if (costs.count(k) && costs[k] <= g)
                continue;
            costs[k] = g;
            parent[k] = n.p;
            int priority = g;
            if (algorithm == "astar")
                priority += distance(p, goal);
            if (algorithm == "greedy")
                priority = distance(p, goal);
            if (algorithm == "dfs")
                priority = -order;
            q.push({priority, order++, g, p});
        }
    }
    return {};
}
void stepGameplay(Snapshot &state_, const std::vector<Command> &commands) {
    auto emit = [&](Id a, Id t, const std::string &k, Vec3 p, float v = 1.f) {
        state_.events.push_back({std::uint64_t(state_.tick) * 1000 + state_.events.size() + 1,
                                 state_.tick, a, t, k, p, v});
    };
    if (finished(state_))
        return;
    state_.events.clear();
    ++state_.tick;
    std::map<Id, Command> valid;
    std::set<Id> duplicates;
    for (auto c : commands) {
        if (valid.count(c.actor))
            duplicates.insert(c.actor);
        else
            valid[c.actor] = c;
    }
    for (auto id : duplicates)
        valid.erase(id);
    std::map<Id, int> damage;
    std::map<Id, Id> plates;
    for (auto &a : state_.actors) {
        if (!active(a))
            continue;
        auto it = valid.find(a.id);
        if (it == valid.end() || it->second.cancel)
            continue;
        auto c = it->second;
        if (c.move.y == 0 && c.move.x >= -1 && c.move.x <= 1 && c.move.z >= -1 && c.move.z <= 1 &&
            std::abs(c.move.x) + std::abs(c.move.z) == 1) {
            a.facing = c.move;
            for (int n = 0; n < (c.sprint && !c.crouch ? 2 : 1); ++n) {
                auto p = plus(a.position, c.move);
                if (blocked(state_, p))
                    break;
                a.position = p;
                ++a.pathCost;
            }
            if (c.sprint && !c.crouch)
                emit(a.id, 0, "noise", a.position);
        }
        if (c.offer && a.ammo > 0) {
            for (auto &recipient : state_.actors)
                if (recipient.id == c.target && recipient.id != a.id && active(recipient) &&
                    distance(a.position, recipient.position) <= 1 &&
                    lineOfSight(state_, a.position, recipient.position)) {
                    --a.ammo;
                    ++recipient.ammo;
                    emit(a.id, recipient.id, "offer", a.position);
                    break;
                }
        }
        if (c.interact) {
            for (auto &o : state_.objects) {
                if (c.target && c.target != o.id)
                    continue;
                if (!o.available || distance(a.position, o.position) > 1 ||
                    !lineOfSight(state_, a.position, o.position))
                    continue;
                if (o.type == "plate") {
                    if (a.position == o.position)
                        plates[o.id] = a.id;
                    continue;
                }
                if (o.type == "weapon") {
                    a.weapon = true;
                    a.ammo += 6;
                } else if (o.type == "ammo")
                    a.ammo += 8;
                else if (o.type == "medkit") {
                    if (a.health >= 100)
                        continue;
                    a.health = std::min(100, a.health + 40);
                }
                o.available = false;
                ++a.pickups;
                emit(a.id, o.id, "pickup", a.position);
                break;
            }
        }
        if ((c.aim || c.fire) && a.weapon) {
            auto t = actor(state_, c.target);
            if (t && t->id != a.id && active(*t) && distance(a.position, t->position) <= 6 &&
                lineOfSight(state_, a.position, t->position)) {
                emit(a.id, t->id, "aim", a.position);
                if (c.fire && a.ammo > 0) {
                    --a.ammo;
                    damage[t->id] += 25;
                    bool retaliation = std::find(a.attackers.begin(), a.attackers.end(), t->id) !=
                                       a.attackers.end();
                    if (t->role == "survivor" && !retaliation)
                        ++a.firstAttacks;
                    for (auto &victim : state_.actors)
                        if (victim.id == t->id &&
                            std::find(victim.attackers.begin(), victim.attackers.end(), a.id) ==
                                victim.attackers.end())
                            victim.attackers.push_back(a.id);
                    emit(a.id, t->id, "attack", a.position, 0.25f);
                }
            }
        }
        if (a.role == "runner")
            emit(a.id, 0, "noise", a.position);
    }
    for (auto &a : state_.actors) {
        if (damage.count(a.id)) {
            a.damageReceived += damage[a.id];
            a.health = std::max(0, a.health - damage[a.id]);
            if (a.health == 0) {
                a.alive = false;
                emit(a.id, 0, "death", a.position);
            }
        }
        if (active(a) && a.role == "survivor" && a.position == state_.goal) {
            a.arrived = true;
            a.arrivalTick = state_.tick;
            emit(a.id, 0, "arrival", a.position);
        }
    }
    for (auto it = plates.begin(); it != plates.end();) {
        auto holder = actor(state_, it->second);
        if (!holder || !active(*holder))
            it = plates.erase(it);
        else
            ++it;
    }
    if (!state_.gateOpen) {
        if (plates.count(201) && plates.count(202) && plates[201] != plates[202])
            ++state_.gateHold;
        else
            state_.gateHold = 0;
        if (state_.gateHold >= 2) {
            state_.gateOpen = true;
            emit(plates[201], plates[202], "help", {8, 0, 5});
            emit(plates[202], plates[201], "help", {8, 0, 5});
        }
    }
}
Observation observe(const Snapshot &s, Id id, Memory &m) {
    Observation o;
    o.agent = id;
    auto a = actor(s, id);
    if (!a || !active(*a)) {
        o.valid = {0, 0, 0, 0, 0, 1};
        return o;
    }
    for (auto &b : s.actors)
        if (b.id != id && b.alive && distance(a->position, b.position) <= 7 &&
            lineOfSight(s, a->position, b.position)) {
            o.visibleActors.push_back(b.id);
            auto &belief = m.beliefs[b.id];
            belief.position = b.position;
            belief.lastSeen = s.tick;
        }
    for (auto &e : s.events) {
        bool heard =
            (e.kind == "attack" || e.kind == "noise") && distance(a->position, e.position) <= 8;
        bool seen =
            distance(a->position, e.position) <= 7 && lineOfSight(s, a->position, e.position);
        if (!(heard || seen) || std::any_of(m.history.begin(), m.history.end(),
                                            [&](const Event &h) { return h.id == e.id; }))
            continue;
        m.history.push_back(e);
        auto &b = m.beliefs[e.actor];
        if (seen) {
            b.lastSeen = s.tick;
            b.position = e.position;
            if (e.kind == "help" || e.kind == "offer")
                b.trust = std::min(1.f, b.trust + 0.3f);
            if (e.kind == "aim" && e.target == id)
                b.threat = std::min(1.f, b.threat + 0.2f);
        }
        if (e.kind == "attack" && (seen || e.target == id))
            b.threat = std::min(1.f, b.threat + 0.5f);
    }
    m.history.erase(std::remove_if(m.history.begin(), m.history.end(),
                                   [&](auto &e) { return s.tick - e.tick > 30; }),
                    m.history.end());
    double lootDistance = 30, threat = 0, trust = 0;
    bool hostile = false;
    int peers = 0;
    for (auto &b : s.actors)
        if (std::find(o.visibleActors.begin(), o.visibleActors.end(), b.id) !=
            o.visibleActors.end()) {
            auto belief = m.beliefs[b.id];
            threat = std::max(threat, double(belief.threat));
            trust = std::max(trust, double(belief.trust));
            hostile |= belief.threat > 0.2f;
            if (b.role == "survivor")
                ++peers;
        }
    for (auto &obj : s.objects)
        if (distance(a->position, obj.position) <= 7 && lineOfSight(s, a->position, obj.position)) {
            if (obj.available) {
                o.visibleObjects.push_back(obj.id);
                if (obj.type != "plate") {
                    lootDistance =
                        std::min(lootDistance, double(distance(a->position, obj.position)));
                    m.loot[obj.id] = {obj, s.tick};
                }
            } else
                m.loot.erase(obj.id);
        }
    for (auto it = m.loot.begin(); it != m.loot.end();)
        if (s.tick - it->second.second > 20)
            it = m.loot.erase(it);
        else
            ++it;
    bool loot = lootDistance < 30, gate = !s.gateOpen && a->position.x < 9 && s.tick < 35;
    o.valid = {1, int(loot), int(gate), int(hostile), int(hostile && a->weapon && a->ammo > 0), 1};
    o.presence = {int(loot), int(!o.visibleActors.empty()), int(hostile), int(gate)};
    o.features = {a->health / 100.,
                  unit(a->ammo / 20.),
                  double(a->weapon),
                  1 - unit(lootDistance / 30),
                  threat,
                  trust,
                  1 - unit(distance(a->position, s.goal) / 30.),
                  (s.goal.x - a->position.x + 19) / 38.,
                  (s.goal.z - a->position.z + 13) / 26.,
                  double(s.gateOpen),
                  1 - unit(distance(a->position, {8, 0, 5}) / 30.),
                  unit(peers / 4.),
                  unit(double(s.tick) / s.maxTicks),
                  unit(a->pickups / 5.),
                  double(!m.history.empty()),
                  unit(m.loot.size() / 5.)};
    return o;
}
AgentIntent baseline(const Snapshot &s, const Observation &o, const Memory &) {
    auto a = actor(s, o.agent);
    Intent intent = Intent::Progress;
    if (o.valid[3])
        intent = o.valid[4] ? Intent::Defend : Intent::Avoid;
    else if (o.valid[1] && a && (!a->weapon || a->ammo < 2 || a->health < 65))
        intent = Intent::Acquire;
    return {o.agent, intent, 0};
}
Command resolve(const Snapshot &s, const AgentIntent &i, const Observation &o, const Memory &m) {
    Command c;
    c.actor = i.agent;
    auto a = actor(s, i.agent);
    if (!a || !active(*a))
        return c;
    auto k = static_cast<unsigned>(i.intent);
    if (k >= 6 || !o.valid[k])
        return resolve(s, {i.agent, Intent::Progress, 0}, o, m);
    Vec3 target = s.goal;
    bool chosen = false;
    if (i.intent == Intent::Acquire) {
        int best = 999;
        for (auto id : o.visibleObjects) {
            auto obj = object(s, id);
            if (!obj || obj->type == "plate" || (i.target && i.target != id) ||
                (obj->type == "medkit" && a->health == 100))
                continue;
            auto path = navigate(s, a->position, obj->position);
            int d = distance(a->position, obj->position);
            if (d > 1 && path.empty())
                continue;
            int cost = int(path.size());
            if (cost < best) {
                best = cost;
                target = obj->position;
                c.target = id;
                chosen = true;
            }
        }
        if (chosen && distance(a->position, target) <= 1) {
            c.interact = true;
            return c;
        }
    }
    if (i.intent == Intent::Cooperate) {
        Id plate = a->id % 2 ? 201 : 202;
        auto obj = object(s, plate);
        if (obj) {
            target = obj->position;
            c.target = plate;
            if (a->position == target) {
                c.interact = true;
                return c;
            }
        }
    }
    if (i.intent == Intent::Defend || i.intent == Intent::Avoid) {
        const Actor *danger = nullptr;
        int nearest = 999;
        for (auto id : o.visibleActors) {
            auto b = actor(s, id);
            auto it = m.beliefs.find(id);
            if (b && it != m.beliefs.end() && it->second.threat > 0.2f &&
                distance(a->position, b->position) < nearest && (!i.target || i.target == id)) {
                nearest = distance(a->position, b->position);
                danger = b;
            }
        }
        if (danger) {
            if (i.intent == Intent::Defend && nearest <= 6) {
                c.target = danger->id;
                c.fire = true;
                return c;
            }
            if (i.intent == Intent::Avoid) {
                int best = nearest;
                for (auto d : dirs) {
                    auto p = plus(a->position, d);
                    if (!blocked(s, p) && distance(p, danger->position) > best) {
                        best = distance(p, danger->position);
                        c.move = d;
                    }
                }
                if (best > nearest)
                    return c;
            }
        }
    }
    if (i.intent == Intent::Explore) {
        std::vector<Vec3> candidates;
        for (int z = 1; z < int(s.map.size()) - 1; ++z)
            for (int x = 1; x < int(s.map[z].size()) - 1; ++x)
                if (!blocked(s, {x, 0, z}))
                    candidates.push_back({x, 0, z});
        if (!candidates.empty())
            target = candidates[(s.seed + a->id * 17 + (s.tick / 8) * 31) % candidates.size()];
    }
    auto path = navigate(s, a->position, target);
    if (!path.empty())
        c.move = {path[0].x - a->position.x, 0, path[0].z - a->position.z};
    return c;
}
bool finished(const Snapshot &s) {
    return s.tick >= s.maxTicks || std::none_of(s.actors.begin(), s.actors.end(), [](auto &a) {
               return a.role == "survivor" && active(a);
           });
}
std::vector<Result> results(const Snapshot &s) {
    std::vector<Result> r;
    for (auto &a : s.actors)
        if (a.role == "survivor") {
            double fitness = (a.arrived ? 1000 : 0) + (a.alive ? 50 : 0) + a.health * 0.5 -
                             (a.arrived ? a.arrivalTick : s.tick) * 0.5 - a.firstAttacks * 100.;
            r.push_back({a.id, a.arrived, a.alive, a.arrived ? a.arrivalTick : s.tick, a.health,
                         a.damageReceived, a.firstAttacks, a.pathCost, a.pickups, fitness});
        }
    return r;
}
std::vector<Observation> Simulation::observations() {
    auto s = world.snapshot();
    std::vector<Observation> v;
    for (auto &a : s.actors)
        if (a.role == "survivor" && active(a))
            v.push_back(observe(s, a.id, memories[a.id]));
    return v;
}
void Simulation::step(const std::vector<AgentIntent> &intents, const Command *human) {
    auto s = world.snapshot();
    auto obs = observations();
    std::vector<Command> cmds;
    for (auto &o : obs) {
        if (human && human->actor == o.agent) {
            cmds.push_back(*human);
            continue;
        }
        auto i = baseline(s, o, memories[o.agent]);
        int count = 0;
        for (auto &proposed : intents)
            if (proposed.agent == o.agent) {
                i = proposed;
                ++count;
            }
        if (count > 1)
            i = baseline(s, o, memories[o.agent]);
        cmds.push_back(resolve(s, i, o, memories[o.agent]));
    }
    for (auto &a : s.actors) {
        if (!active(a) || a.role == "survivor")
            continue;
        Command c;
        c.actor = a.id;
        if (a.role == "aggressor") {
            const Actor *t = nullptr;
            int best = 7;
            for (auto &b : s.actors)
                if (b.role == "survivor" && active(b) && distance(a.position, b.position) < best &&
                    lineOfSight(s, a.position, b.position)) {
                    t = &b;
                    best = distance(a.position, b.position);
                }
            if (t) {
                c.target = t->id;
                c.fire = true;
            }
        }
        if (a.role == "runner") {
            c.move = dirs[(s.tick / 3) % 4];
        }
        if (a.role == "scavenger") {
            const Object *t = nullptr;
            int best = 999;
            for (auto &ob : s.objects)
                if (ob.available && ob.type != "plate" &&
                    distance(a.position, ob.position) < best) {
                    best = distance(a.position, ob.position);
                    t = &ob;
                }
            if (t) {
                c.target = t->id;
                if (best <= 1)
                    c.interact = true;
                else {
                    auto p = navigate(s, a.position, t->position);
                    if (!p.empty())
                        c.move = {p[0].x - a.position.x, 0, p[0].z - a.position.z};
                }
            }
        }
        cmds.push_back(c);
    }
    world.step(cmds);
}
} // namespace survival
