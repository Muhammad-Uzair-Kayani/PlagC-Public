#include "pgcpch.h"
#include <PlagC/Editor/PlaySession.h>
#include <PlagC/Thread.h>

namespace PlagC::Editor {

void PlaySession::Start(const SceneDocument& authoring) {
    Thread::RequireMain();
    if (state_ != State::Stopped)
        throw std::logic_error("Editor play session is already running");

    auto clone = authoring.CloneForRuntime();
    runtime_ = std::move(clone);
    state_ = State::Playing;
    stepRequested_ = false;
    ++generation_;
}

void PlaySession::Stop() {
    Thread::RequireMain();
    if (state_ == State::Stopped)
        return;
    runtime_.reset();
    state_ = State::Stopped;
    stepRequested_ = false;
    ++generation_;
}

void PlaySession::Pause() {
    Thread::RequireMain();
    if (state_ == State::Stopped)
        throw std::logic_error("Cannot pause a stopped editor play session");
    state_ = State::Paused;
    stepRequested_ = false;
}

void PlaySession::Resume() {
    Thread::RequireMain();
    if (state_ == State::Stopped)
        throw std::logic_error("Cannot resume a stopped editor play session");
    state_ = State::Playing;
    stepRequested_ = false;
}

void PlaySession::TogglePause() {
    Thread::RequireMain();
    if (state_ == State::Stopped)
        throw std::logic_error("Cannot pause a stopped editor play session");
    if (state_ == State::Paused)
        Resume();
    else
        Pause();
}

void PlaySession::RequestStep() {
    Thread::RequireMain();
    if (state_ != State::Paused)
        throw std::logic_error("Frame stepping requires a paused editor play session");
    stepRequested_ = true;
}

bool PlaySession::ConsumeStep() {
    Thread::RequireMain();
    if (state_ != State::Paused || !stepRequested_)
        return false;
    stepRequested_ = false;
    return true;
}

PlaySession::State PlaySession::CurrentState() const {
    Thread::RequireMain();
    return state_;
}

bool PlaySession::Running() const {
    Thread::RequireMain();
    return state_ != State::Stopped;
}

bool PlaySession::Paused() const {
    Thread::RequireMain();
    return state_ == State::Paused;
}

std::uint64_t PlaySession::Generation() const {
    Thread::RequireMain();
    return generation_;
}

World& PlaySession::RuntimeWorld() {
    Thread::RequireMain();
    if (!runtime_)
        throw std::logic_error("Editor play session has no runtime world");
    return *runtime_->world_;
}

const World& PlaySession::RuntimeWorld() const {
    Thread::RequireMain();
    if (!runtime_)
        throw std::logic_error("Editor play session has no runtime world");
    return *runtime_->world_;
}

const SceneDocument& PlaySession::RuntimeDocument() const {
    Thread::RequireMain();
    if (!runtime_)
        throw std::logic_error("Editor play session has no runtime document");
    return *runtime_;
}

} // namespace PlagC::Editor
