#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace survival {
using Id = std::uint32_t;
struct Vec3 {
    int x{}, y{}, z{};
    bool operator==(const Vec3 &) const = default;
};
int distance(Vec3 a, Vec3 b);
enum class Intent { Explore, Acquire, Cooperate, Avoid, Defend, Progress };
std::string name(Intent);
Intent parseIntent(const std::string &);
struct Actor {
    Id id{};
    Vec3 position{}, facing{1, 0, 0};
    int health{100}, ammo{};
    bool weapon{}, alive{true}, arrived{};
    std::string role{"survivor"};
    int arrivalTick{-1}, pathCost{}, damageReceived{}, firstAttacks{}, pickups{};
    std::vector<Id> attackers;
};
struct Object {
    Id id{};
    std::string type;
    Vec3 position;
    bool available{true};
};
struct Event {
    std::uint64_t id{};
    int tick{};
    Id actor{}, target{};
    std::string kind;
    Vec3 position;
    float intensity{1};
};
struct Belief {
    float trust{}, threat{};
    int lastSeen{-1};
    Vec3 position;
};
struct Memory {
    std::map<Id, Belief> beliefs;
    std::vector<Event> history;
    std::map<Id, std::pair<Object, int>> loot;
};
struct Scenario {
    std::uint32_t seed{1};
    int maxTicks{160};
    bool aggressor{}, runner{}, scavenger{};
    std::string name{"peaceful"};
};
struct Snapshot {
    std::uint32_t seed{};
    int tick{}, maxTicks{}, gateHold{};
    bool gateOpen{};
    Vec3 goal{17, 0, 6};
    std::vector<std::string> map;
    std::vector<Actor> actors;
    std::vector<Object> objects;
    std::vector<Event> events;
};
struct Command {
    Id actor{};
    Vec3 move{};
    bool interact{}, fire{}, aim{}, sprint{}, crouch{}, cancel{};
    Id target{};
    bool offer{};
};
struct Observation {
    Id agent{};
    std::array<double, 16> features{};
    std::array<int, 6> valid{1, 0, 0, 0, 0, 1};
    std::array<int, 4> presence{};
    std::vector<Id> visibleActors, visibleObjects;
};
struct AgentIntent {
    Id agent{};
    Intent intent{Intent::Progress};
    Id target{};
};
struct Result {
    Id agent{};
    bool arrived{}, alive{};
    int ticks{}, health{}, damage{}, firstAttacks{}, pathCost{}, pickups{};
    double fitness{};
};
class IWorldPort {
  public:
    virtual ~IWorldPort() = default;
    virtual Snapshot snapshot() const = 0;
    virtual void reset(const Scenario &) = 0;
    virtual void step(const std::vector<Command> &) = 0;
};
class IAITransport {
  public:
    virtual ~IAITransport() = default;
    virtual std::vector<AgentIntent> infer(const std::vector<Observation> &, std::uint64_t, int,
                                           int) = 0;
};
class IInputSource {
  public:
    virtual ~IInputSource() = default;
    virtual Command read(Id) = 0;
};
class IEventSink {
  public:
    virtual ~IEventSink() = default;
    virtual void write(const Event &) = 0;
};
class IScenarioSource {
  public:
    virtual ~IScenarioSource() = default;
    virtual Scenario load(std::uint32_t) = 0;
};
class IRenderSink {
  public:
    virtual ~IRenderSink() = default;
    virtual void render(const Snapshot &) = 0;
};
bool blocked(const Snapshot &, Vec3);
bool lineOfSight(const Snapshot &, Vec3, Vec3);
std::vector<Vec3> navigate(const Snapshot &, Vec3, Vec3, const std::string &algorithm = "astar");
Observation observe(const Snapshot &, Id, Memory &);
AgentIntent baseline(const Snapshot &, const Observation &, const Memory &);
Command resolve(const Snapshot &, const AgentIntent &, const Observation &, const Memory &);
void stepGameplay(Snapshot &, const std::vector<Command> &);
bool finished(const Snapshot &);
std::vector<Result> results(const Snapshot &);
class SyntheticWorld final : public IWorldPort {
  public:
    explicit SyntheticWorld(const Scenario &s = {}) { reset(s); }
    Snapshot snapshot() const override { return state_; }
    void reset(const Scenario &) override;
    void step(const std::vector<Command> &) override;

  private:
    Snapshot state_;
};
class Simulation {
  public:
    explicit Simulation(IWorldPort &w) : world(w) {}
    IWorldPort &world;
    std::map<Id, Memory> memories;
    std::vector<Observation> observations();
    void step(const std::vector<AgentIntent> &intents = {}, const Command *human = nullptr);
};
} // namespace survival
