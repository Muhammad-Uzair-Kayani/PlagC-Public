#pragma once
#include <PlagC/Editor/CommandHistory.h>
#include <PlagC/World/World.h>
namespace PlagC::Editor {
// Owns the authoring world. Simulation must use a separate world.
class SceneDocument {
public:
    explicit SceneDocument(std::unique_ptr<World>, std::size_t historyCapacity = 256);
    ~SceneDocument();
    SceneDocument(const SceneDocument&) = delete;
    SceneDocument& operator=(const SceneDocument&) = delete;
    const World& Scene() const;
    Entity Selection() const;
    void Select(Entity);
    void Rename(Entity, std::string);
    void SetTransform(Entity, const Transform&);
    bool Undo();
    bool Redo();
    bool Dirty() const;
    void MarkSaved();
    std::string_view UndoLabel() const;
    std::string_view RedoLabel() const;
private:
    std::unique_ptr<World> world_;
    CommandHistory history_; // destroyed before referenced world
    Entity selection_{};
};
}
