#include <PlagC/Editor/CommandHistory.h>
#include <cassert>
using namespace PlagC::Editor;
struct Edit final : Command {
    int &v; int before, after; bool failApply=false, failUndo=false;
    Edit(int &value,int next):v(value),before(value),after(next) {}
    std::string_view Label() const noexcept override { return "Edit"; }
    void Apply() override { if(failApply) throw std::runtime_error("apply"); v=after; }
    void Revert() override { if(failUndo) throw std::runtime_error("undo"); v=before; }
};
int main() {
    int v=0; CommandHistory h(2); assert(!h.Dirty() && !h.Undo() && !h.Redo());
    h.Execute(std::make_unique<Edit>(v,1)); h.MarkSaved();
    h.Execute(std::make_unique<Edit>(v,2)); assert(h.Dirty() && h.Undo() && v==1 && !h.Dirty());
    auto bad=std::make_unique<Edit>(v,9); bad->failApply=true;
    try { h.Execute(std::move(bad)); assert(false); } catch(const std::runtime_error&) {}
    assert(v==1 && !h.Dirty() && h.Redo() && v==2);
    h.MarkSaved(); assert(h.Undo()); h.Execute(std::make_unique<Edit>(v,3)); assert(h.Dirty() && !h.Redo());
    h.Execute(std::make_unique<Edit>(v,4)); assert(h.Undo() && v==3 && h.Undo() && v==1 && !h.Undo());
    h.Clear(); auto badUndo=std::make_unique<Edit>(v,5); badUndo->failUndo=true; h.Execute(std::move(badUndo));
    try { h.Undo(); assert(false); } catch(const std::runtime_error&) {}
    assert(v==5 && h.UndoLabel()=="Edit" && h.Dirty()); h.Clear(); assert(!h.Dirty() && !h.Undo());
}
