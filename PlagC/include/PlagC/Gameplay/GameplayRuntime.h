#pragma once

#include <PlagC/Events.h>
#include <PlagC/Input/Input.h>
#include <PlagC/Physics/PhysicsTypes.h>
#include <PlagC/World/World.h>
#include <cstddef>
#include <deque>
#include <memory>
#include <vector>

namespace PlagC {

class Engine;
class PhysicsSystem;

struct GameplayContext {
    Engine& engine;
    World& world;
    PhysicsSystem& physics;
    const InputState& input;
};

// Engine-owned gameplay contract used by games and the Editor Play runtime.
// Systems execute deterministically in registration order; shutdown executes in reverse order.
class GameplaySystem {
public:
    virtual ~GameplaySystem() = default;
    virtual void OnStart(GameplayContext&) {}
    virtual void OnStop(Engine&) {}
    virtual void OnPause(GameplayContext&) {}
    virtual void OnResume(GameplayContext&) {}
    virtual void OnInput(GameplayContext&, const Event&) {}
    virtual void OnUpdate(GameplayContext&, double) {}
    virtual void OnFixedUpdate(GameplayContext&, double) {}
    virtual void OnTrigger(GameplayContext&, Entity sensor, Entity other, TriggerPhase) {}
    virtual void OnCollision(GameplayContext&, Entity a, Entity b, CollisionPhase) {}
    virtual void OnLateUpdate(GameplayContext&, double) {}
};

class GameplayRuntime final {
public:
    GameplayRuntime() = default;
    ~GameplayRuntime() = default;
    GameplayRuntime(const GameplayRuntime&) = delete;
    GameplayRuntime& operator=(const GameplayRuntime&) = delete;

    void Initialize(EventBus&);
    void Shutdown(Engine&) noexcept;

    void AddSystem(std::shared_ptr<GameplaySystem>);
    void ClearSystems();

    void SetInputRouting(bool keyboardEnabled, bool mouseEnabled);
    bool KeyboardInputEnabled() const noexcept { return keyboardInputEnabled_; }
    bool MouseInputEnabled() const noexcept { return mouseInputEnabled_; }
    bool Running() const noexcept { return running_; }
    const InputState& Input() const noexcept { return input_; }

    void Start(GameplayContext&);
    void Stop(Engine&);
    void Pause(GameplayContext&);
    void Resume(GameplayContext&);
    void DispatchInput(GameplayContext&);
    void Update(GameplayContext&, double frameSeconds);
    void FixedUpdate(GameplayContext&, double fixedSeconds);
    void DispatchPhysicsEvents(GameplayContext&);
    void LateUpdate(GameplayContext&, double frameSeconds);
    void FinishInputFrame() noexcept;

private:
    static bool IsKeyboardEvent(EventType) noexcept;
    static bool IsMouseEvent(EventType) noexcept;
    static void ValidateDelta(double, const char*);
    void RouteInput(const Event&);
    void RequireRunning() const;

    std::vector<std::shared_ptr<GameplaySystem>> systems_;
    std::vector<EventBus::Connection> inputConnections_;
    std::deque<Event> pendingInput_;
    InputState input_{};
    bool initialized_ = false;
    bool running_ = false;
    bool keyboardInputEnabled_ = false;
    bool mouseInputEnabled_ = false;
    static constexpr std::size_t MaxPendingInput = 4096;
};

} // namespace PlagC
