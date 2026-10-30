#include "pgcpch.h"
#include <PlagC/Gameplay/GameplayRuntime.h>
#include <PlagC/Application.h>
#include <PlagC/Physics/PhysicsSystem.h>
#include <PlagC/Thread.h>
#include <exception>
#include <iterator>

namespace PlagC {

namespace {
constexpr EventType RoutedInputEvents[] = {
    EventType::Focus,
    EventType::KeyPressed,
    EventType::KeyReleased,
    EventType::KeyRepeated,
    EventType::TextInput,
    EventType::MouseMoved,
    EventType::MouseScrolled,
    EventType::MousePressed,
    EventType::MouseReleased
};
}

void GameplayRuntime::Initialize(EventBus& events)
{
    Thread::RequireMain();
    if (initialized_)
        return;

    inputConnections_.reserve(std::size(RoutedInputEvents));
    for (const EventType type : RoutedInputEvents)
        inputConnections_.push_back(events.Subscribe(type, [this](const Event& event) { RouteInput(event); }));
    initialized_ = true;
}

void GameplayRuntime::Shutdown(Engine& engine) noexcept
{
    Thread::RequireMain();
    if (running_)
    {
        try { Stop(engine); }
        catch (...) {}
    }
    inputConnections_.clear();
    pendingInput_.clear();
    input_.Reset();
    initialized_ = false;
    keyboardInputEnabled_ = false;
    mouseInputEnabled_ = false;
}

void GameplayRuntime::AddSystem(std::shared_ptr<GameplaySystem> system)
{
    Thread::RequireMain();
    if (running_)
        throw std::logic_error("Cannot register gameplay systems while runtime is running");
    if (!system)
        throw std::invalid_argument("Gameplay system is null");
    systems_.push_back(std::move(system));
}

void GameplayRuntime::ClearSystems()
{
    Thread::RequireMain();
    if (running_)
        throw std::logic_error("Cannot clear gameplay systems while runtime is running");
    systems_.clear();
}

bool GameplayRuntime::IsKeyboardEvent(EventType type) noexcept
{
    return type == EventType::KeyPressed || type == EventType::KeyReleased ||
           type == EventType::KeyRepeated || type == EventType::TextInput;
}

bool GameplayRuntime::IsMouseEvent(EventType type) noexcept
{
    return type == EventType::MouseMoved || type == EventType::MouseScrolled ||
           type == EventType::MousePressed || type == EventType::MouseReleased;
}

void GameplayRuntime::SetInputRouting(bool keyboardEnabled, bool mouseEnabled)
{
    Thread::RequireMain();
    if (keyboardInputEnabled_ && !keyboardEnabled)
    {
        input_.ResetKeyboard();
        std::erase_if(pendingInput_, [](const Event& event) { return GameplayRuntime::IsKeyboardEvent(event.type); });
    }
    if (mouseInputEnabled_ && !mouseEnabled)
    {
        input_.ResetMouse();
        std::erase_if(pendingInput_, [](const Event& event) { return GameplayRuntime::IsMouseEvent(event.type); });
    }
    keyboardInputEnabled_ = keyboardEnabled;
    mouseInputEnabled_ = mouseEnabled;
}

void GameplayRuntime::RouteInput(const Event& event)
{
    Thread::RequireMain();
    // Routing is enabled by the editor only after Play is requested. Accepting the
    // pre-Start pump preserves the first Play frame without inheriting Edit-mode input.
    if (!running_ && !keyboardInputEnabled_ && !mouseInputEnabled_)
        return;

    if (event.type == EventType::Focus)
    {
        if (!event.code)
        {
            input_.Reset();
            pendingInput_.clear();
        }
    }
    else if (IsKeyboardEvent(event.type))
    {
        if (!keyboardInputEnabled_)
            return;
        input_.Apply(event);
    }
    else if (IsMouseEvent(event.type))
    {
        if (!mouseInputEnabled_)
            return;
        input_.Apply(event);
    }
    else
        return;

    if (pendingInput_.size() == MaxPendingInput)
        pendingInput_.pop_front();
    pendingInput_.push_back(event);
}

void GameplayRuntime::Start(GameplayContext& context)
{
    Thread::RequireMain();
    if (!initialized_)
        throw std::logic_error("Gameplay runtime must be initialized before Start");
    if (running_)
        throw std::logic_error("Gameplay runtime is already running");

    running_ = true;
    std::size_t started = 0;
    try
    {
        for (auto& system : systems_)
        {
            system->OnStart(context);
            ++started;
        }
    }
    catch (...)
    {
        for (std::size_t i = started; i > 0; --i)
        {
            try { systems_[i - 1]->OnStop(context.engine); }
            catch (...) {}
        }
        running_ = false;
        input_.Reset();
        pendingInput_.clear();
        throw;
    }
}

void GameplayRuntime::Stop(Engine& engine)
{
    Thread::RequireMain();
    if (!running_)
        return;

    std::exception_ptr firstError;
    for (std::size_t i = systems_.size(); i > 0; --i)
    {
        try { systems_[i - 1]->OnStop(engine); }
        catch (...) { if (!firstError) firstError = std::current_exception(); }
    }

    running_ = false;
    keyboardInputEnabled_ = false;
    mouseInputEnabled_ = false;
    input_.Reset();
    pendingInput_.clear();
    if (firstError)
        std::rethrow_exception(firstError);
}

void GameplayRuntime::Pause(GameplayContext& context)
{
    Thread::RequireMain();
    RequireRunning();
    for (auto& system : systems_)
        system->OnPause(context);
}

void GameplayRuntime::Resume(GameplayContext& context)
{
    Thread::RequireMain();
    RequireRunning();
    for (auto& system : systems_)
        system->OnResume(context);
}

void GameplayRuntime::DispatchInput(GameplayContext& context)
{
    Thread::RequireMain();
    RequireRunning();
    const auto input = std::move(pendingInput_);
    pendingInput_.clear();
    for (const auto& event : input)
        for (auto& system : systems_)
            system->OnInput(context, event);
}

void GameplayRuntime::ValidateDelta(double value, const char* label)
{
    if (!std::isfinite(value) || value < 0.0)
        throw std::invalid_argument(std::string(label) + " must be finite and non-negative");
}

void GameplayRuntime::Update(GameplayContext& context, double frameSeconds)
{
    Thread::RequireMain();
    RequireRunning();
    ValidateDelta(frameSeconds, "Gameplay frame delta");
    for (auto& system : systems_)
        system->OnUpdate(context, frameSeconds);
}

void GameplayRuntime::FixedUpdate(GameplayContext& context, double fixedSeconds)
{
    Thread::RequireMain();
    RequireRunning();
    ValidateDelta(fixedSeconds, "Gameplay fixed delta");
    for (auto& system : systems_)
        system->OnFixedUpdate(context, fixedSeconds);
}

void GameplayRuntime::DispatchPhysicsEvents(GameplayContext& context)
{
    Thread::RequireMain();
    RequireRunning();

    for (const auto& event : context.physics.TriggerEvents())
    {
        const Entity sensor = context.physics.EntityFor(event.sensor);
        const Entity other = context.physics.EntityFor(event.other);
        if (!sensor || !other)
            continue;
        for (auto& system : systems_)
            system->OnTrigger(context, sensor, other, event.phase);
    }

    for (const auto& event : context.physics.CollisionEvents())
    {
        const Entity a = context.physics.EntityFor(event.a);
        const Entity b = context.physics.EntityFor(event.b);
        if (!a || !b)
            continue;
        for (auto& system : systems_)
            system->OnCollision(context, a, b, event.phase);
    }
}

void GameplayRuntime::LateUpdate(GameplayContext& context, double frameSeconds)
{
    Thread::RequireMain();
    RequireRunning();
    ValidateDelta(frameSeconds, "Gameplay late-update delta");
    for (auto& system : systems_)
        system->OnLateUpdate(context, frameSeconds);
}

void GameplayRuntime::FinishInputFrame() noexcept
{
    pendingInput_.clear();
    input_.BeginFrame();
}

void GameplayRuntime::RequireRunning() const
{
    if (!running_)
        throw std::logic_error("Gameplay runtime is not running");
}

} // namespace PlagC
