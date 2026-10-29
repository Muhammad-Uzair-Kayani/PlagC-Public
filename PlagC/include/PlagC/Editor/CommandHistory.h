#pragma once
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace PlagC::Editor {

class Command {
public:
    virtual ~Command() = default;
    virtual std::string_view Label() const noexcept = 0;
    virtual void Apply() = 0;
    virtual void Revert() = 0;
};

// Main-thread history owned by SceneDocument. Command callbacks are never
// allowed to mutate this history recursively. Apply/Revert must be atomic:
// if either throws, the command cursor and dirty/saved state remain unchanged.
class CommandHistory {
public:
    explicit CommandHistory(std::size_t capacity = 256) : capacity_(capacity) {
        if (!capacity_)
            throw std::invalid_argument("History capacity must be positive");
        commands_.reserve(capacity_);
    }

    CommandHistory(const CommandHistory&) = delete;
    CommandHistory& operator=(const CommandHistory&) = delete;
    CommandHistory(CommandHistory&&) = delete;
    CommandHistory& operator=(CommandHistory&&) = delete;

    void Execute(std::unique_ptr<Command> command) {
        if (!command)
            throw std::invalid_argument("Null editor command");
        Guard guard(busy_);
        command->Apply();
        Commit(std::move(command));
    }

    // Interaction transactions (gizmo/Inspector drags) can preview directly in
    // the authoring world and record one final command when the gesture ends.
    void RecordApplied(std::unique_ptr<Command> command) {
        if (!command)
            throw std::invalid_argument("Null editor command");
        Guard guard(busy_);
        Commit(std::move(command));
    }

    bool Undo() {
        Guard guard(busy_);
        if (!cursor_)
            return false;
        commands_[cursor_ - 1]->Revert();
        --cursor_;
        return true;
    }

    bool Redo() {
        Guard guard(busy_);
        if (cursor_ == commands_.size())
            return false;
        commands_[cursor_]->Apply();
        ++cursor_;
        return true;
    }

    bool CanUndo() const noexcept { return cursor_ != 0; }
    bool CanRedo() const noexcept { return cursor_ < commands_.size(); }
    bool Busy() const noexcept { return busy_; }

    std::string_view UndoLabel() const noexcept {
        return cursor_ ? commands_[cursor_ - 1]->Label() : std::string_view{};
    }
    std::string_view RedoLabel() const noexcept {
        return cursor_ < commands_.size() ? commands_[cursor_]->Label() : std::string_view{};
    }

    void MarkSaved() {
        RequireIdle();
        saved_ = cursor_;
    }

    bool Dirty() const noexcept { return saved_ != cursor_; }

    void Clear() {
        RequireIdle();
        commands_.clear();
        cursor_ = saved_ = 0;
    }

private:
    class Guard {
    public:
        explicit Guard(bool& busy) : busy_(busy) {
            if (busy_)
                throw std::logic_error("Editor command history re-entry");
            busy_ = true;
        }
        ~Guard() { busy_ = false; }
        Guard(const Guard&) = delete;
        Guard& operator=(const Guard&) = delete;
    private:
        bool& busy_;
    };

    void RequireIdle() const {
        if (busy_)
            throw std::logic_error("Editor command history re-entry");
    }

    void Commit(std::unique_ptr<Command> command) {
        if (saved_ > cursor_)
            saved_ = invalid;

        commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(cursor_), commands_.end());

        if (commands_.size() == capacity_) {
            commands_.erase(commands_.begin());
            --cursor_;
            if (saved_ != invalid)
                saved_ = saved_ == 0 ? invalid : saved_ - 1;
        }

        commands_.push_back(std::move(command));
        ++cursor_;
    }

    static constexpr std::size_t invalid = static_cast<std::size_t>(-1);
    std::size_t capacity_;
    std::size_t cursor_ = 0;
    std::size_t saved_ = 0;
    bool busy_ = false;
    std::vector<std::unique_ptr<Command>> commands_;
};

} // namespace PlagC::Editor
