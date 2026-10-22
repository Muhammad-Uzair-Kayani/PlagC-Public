#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>
#include <utility>

namespace PlagC {
enum class EventType { Close, Resize, FramebufferResize, Move, Focus, Minimize, KeyPressed, KeyReleased, KeyRepeated, TextInput, MouseMoved, MouseScrolled, MousePressed, MouseReleased, User };
inline const char* EventName(EventType type) {
    constexpr const char* names[] = {"Close", "Resize", "FramebufferResize", "Move", "Focus", "Minimize", "KeyPressed", "KeyReleased", "KeyRepeated", "TextInput", "MouseMoved", "MouseScrolled", "MousePressed", "MouseReleased", "User"};
    auto index = static_cast<unsigned>(type);
    return index < sizeof(names)/sizeof(names[0]) ? names[index] : "Unknown";
}
struct Event {
    EventType type;
    double x = 0, y = 0;
    int code = 0, modifiers = 0;
};
// Dispatch, subscribe and unsubscribe belong to the creating thread. Post is MPSC.
class EventBus {
    struct Slot { std::uint64_t id; EventType type; std::function<void(const Event&)> callback; bool active = true; };
    struct State {
        const std::thread::id owner = std::this_thread::get_id();
        std::vector<std::shared_ptr<Slot>> slots;
        std::uint64_t next = 1;
        unsigned depth = 0;
        void Check() const { if (std::this_thread::get_id() != owner) throw std::logic_error("event owner-thread operation"); }
    };
public:
    class Connection {
        friend class EventBus;
        std::weak_ptr<State> state_;
        std::uint64_t id_ = 0;
        Connection(std::shared_ptr<State> state, std::uint64_t id) : state_(state), id_(id) {}
    public:
        Connection() = default;
        Connection(const Connection&) = delete;
        Connection& operator=(const Connection&) = delete;
        Connection(Connection&& other) noexcept : state_(std::move(other.state_)), id_(std::exchange(other.id_,0)) {}
        Connection& operator=(Connection&& other) noexcept {
            if (this != &other) { Reset(); state_ = std::move(other.state_); id_ = std::exchange(other.id_,0); }
            return *this;
        }
        ~Connection() { Reset(); }
        void Reset() noexcept {
            if (auto s = state_.lock(); s && id_) {
                // Destruction on another thread is a contract violation, including Release.
                if (std::this_thread::get_id() != s->owner) std::terminate();
                for (auto& slot : s->slots) if (slot->id == id_) slot->active = false;
                if (s->depth == 0) std::erase_if(s->slots, [](const auto& slot){ return !slot->active; });
            }
            id_ = 0;
        }
    };
    Connection Subscribe(EventType type, std::function<void(const Event&)> callback) {
        state_->Check();
        const auto id = state_->next++;
        state_->slots.push_back(std::make_shared<Slot>(Slot{id,type,std::move(callback),true}));
        return Connection(state_,id);
    }
    void Dispatch(const Event& event) {
        state_->Check();
        const auto snapshot = state_->slots;
        ++state_->depth;
        try { for (const auto& slot : snapshot) if (slot->active && slot->type == event.type) slot->callback(event); }
        catch (...) { Finish(); throw; }
        Finish();
    }
    bool Post(Event event) {
        std::lock_guard lock(queueMutex_);
        if (queue_.size() == 8192) return false;
        queue_.push_back(event); return true;
    }
    std::size_t Pump() {
        state_->Check();
        std::deque<Event> batch;
        { std::lock_guard lock(queueMutex_); batch.swap(queue_); }
        for (const auto& event : batch) Dispatch(event);
        return batch.size();
    }
private:
    std::shared_ptr<State> state_ = std::make_shared<State>();
    std::mutex queueMutex_;
    std::deque<Event> queue_;
    void Finish() {
        if (--state_->depth == 0) std::erase_if(state_->slots, [](const auto& slot){ return !slot->active; });
    }
};
}
