#pragma once
#include <PlagC/Editor/CommandHistory.h>
#include <PlagC/World/World.h>
#include <filesystem>
#include <functional>
#include <unordered_map>
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
    // Scene-local persistent identity, distinct from the runtime Entity handle.
    std::uint64_t PersistentID(Entity) const;
    using ModelResolver = std::function<AssetHandle<ModelAsset>(const std::string &)>;
    std::string Serialize() const;
    static std::unique_ptr<SceneDocument> Deserialize(std::string_view, ModelResolver = {});
    void Save(const std::filesystem::path &);
    static std::unique_ptr<SceneDocument> Load(const std::filesystem::path &, ModelResolver = {});
private:
    std::unique_ptr<World> world_;
    CommandHistory history_; // destroyed before referenced world
    Entity selection_{};
    std::unordered_map<std::uint64_t, std::uint64_t> persistentIDs_;
};
}
