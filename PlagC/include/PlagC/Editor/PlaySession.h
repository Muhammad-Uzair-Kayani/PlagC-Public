#pragma once

#include <PlagC/Editor/SceneDocument.h>
#include <cstdint>
#include <memory>

namespace PlagC::Editor {

class PlaySession {
public:
    enum class State {
        Stopped,
        Playing,
        Paused
    };

    PlaySession() = default;
    ~PlaySession() = default;

    PlaySession(const PlaySession&) = delete;
    PlaySession& operator=(const PlaySession&) = delete;

    void Start(const SceneDocument&);
    void Stop();
    void Pause();
    void Resume();
    void TogglePause();

    void RequestStep();
    bool ConsumeStep();

    State CurrentState() const;
    bool Running() const;
    bool Paused() const;
    std::uint64_t Generation() const;

    World& RuntimeWorld();
    const World& RuntimeWorld() const;
    const SceneDocument& RuntimeDocument() const;

private:
    std::unique_ptr<SceneDocument> runtime_;
    State state_ = State::Stopped;
    bool stepRequested_ = false;
    std::uint64_t generation_ = 0;
};

} // namespace PlagC::Editor
