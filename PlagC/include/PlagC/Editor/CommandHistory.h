#pragma once
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>
namespace PlagC::Editor {
// Commands own before/after values. Apply/Revert must leave the document
// unchanged on failure. Main-thread only; callbacks must not re-enter history.
class Command {
public:
    virtual ~Command() = default;
    virtual std::string_view Label() const noexcept = 0;
    virtual void Apply() = 0;
    virtual void Revert() = 0;
};
class CommandHistory {
public:
    explicit CommandHistory(std::size_t capacity = 256) : capacity_(capacity) {
        if (!capacity) throw std::invalid_argument("History capacity must be positive");
        commands_.reserve(capacity);
    }
    CommandHistory(const CommandHistory&) = delete;
    CommandHistory& operator=(const CommandHistory&) = delete;
    CommandHistory(CommandHistory&&) = delete;
    CommandHistory& operator=(CommandHistory&&) = delete;
    void Execute(std::unique_ptr<Command> command) {
        if (!command) throw std::invalid_argument("Null editor command");
        command->Apply();
        if (saved_ > cursor_) saved_ = invalid;
        commands_.erase(commands_.begin() + cursor_, commands_.end());
        if (commands_.size() == capacity_) {
            commands_.erase(commands_.begin()); --cursor_;
            if (saved_ != invalid) saved_ = saved_ == 0 ? invalid : saved_ - 1;
        }
        commands_.push_back(std::move(command)); ++cursor_;
    }
    bool Undo() {
        if (!cursor_) return false;
        commands_[cursor_ - 1]->Revert(); --cursor_; return true;
    }
    bool Redo() {
        if (cursor_ == commands_.size()) return false;
        commands_[cursor_]->Apply(); ++cursor_; return true;
    }
    std::string_view UndoLabel() const noexcept { return cursor_ ? commands_[cursor_-1]->Label() : std::string_view{}; }
    std::string_view RedoLabel() const noexcept { return cursor_ < commands_.size() ? commands_[cursor_]->Label() : std::string_view{}; }
    void MarkSaved() noexcept { saved_ = cursor_; }
    bool Dirty() const noexcept { return saved_ != cursor_; }
    void Clear() noexcept { commands_.clear(); cursor_ = saved_ = 0; }
private:
    static constexpr std::size_t invalid = static_cast<std::size_t>(-1);
    std::size_t capacity_, cursor_ = 0, saved_ = 0;
    std::vector<std::unique_ptr<Command>> commands_;
};
}
