#include <PlagC/Editor/CommandHistory.h>
#include <cassert>
#include <stdexcept>
using namespace PlagC::Editor;

struct Edit final : Command {
    int& value;
    int before;
    int after;
    bool failApply = false;
    bool failUndo = false;

    Edit(int& v, int next) : value(v), before(v), after(next) {}
    std::string_view Label() const noexcept override { return "Edit"; }
    void Apply() override {
        if (failApply) throw std::runtime_error("apply");
        value = after;
    }
    void Revert() override {
        if (failUndo) throw std::runtime_error("undo");
        value = before;
    }
};

struct Reenter final : Command {
    CommandHistory& history;
    int& value;
    explicit Reenter(CommandHistory& h, int& v) : history(h), value(v) {}
    std::string_view Label() const noexcept override { return "Reenter"; }
    void Apply() override {
        value = 99;
        history.Undo();
    }
    void Revert() override {}
};

int main() {
    int v = 0;
    CommandHistory h(3);
    assert(!h.Dirty() && !h.CanUndo() && !h.CanRedo());

    h.Execute(std::make_unique<Edit>(v, 1));
    h.MarkSaved();
    h.Execute(std::make_unique<Edit>(v, 2));
    assert(h.Dirty() && h.CanUndo() && h.Undo() && v == 1 && !h.Dirty());

    auto bad = std::make_unique<Edit>(v, 9);
    bad->failApply = true;
    try { h.Execute(std::move(bad)); assert(false); }
    catch (const std::runtime_error&) {}
    assert(v == 1 && !h.Dirty() && h.Redo() && v == 2);

    h.MarkSaved();
    assert(h.Undo());
    h.Execute(std::make_unique<Edit>(v, 3));
    assert(h.Dirty() && !h.CanRedo());

    // RecordApplied is the transaction path used by gizmos: the value is already
    // visible when one undo record is committed.
    auto applied = std::make_unique<Edit>(v, 7);
    v = 7;
    h.RecordApplied(std::move(applied));
    assert(h.Undo() && v == 3);
    assert(h.Redo() && v == 7);

    // Callback re-entry is rejected and does not append the command.
    try { h.Execute(std::make_unique<Reenter>(h, v)); assert(false); }
    catch (const std::logic_error&) {}
    assert(h.UndoLabel() == "Edit");

    h.Clear();
    auto badUndo = std::make_unique<Edit>(v, 5);
    badUndo->failUndo = true;
    h.Execute(std::move(badUndo));
    try { h.Undo(); assert(false); }
    catch (const std::runtime_error&) {}
    assert(v == 5 && h.UndoLabel() == "Edit" && h.Dirty());

    h.Clear();
    assert(!h.Dirty() && !h.Undo() && !h.Redo());
}
