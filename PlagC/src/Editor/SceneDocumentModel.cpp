#include "pgcpch.h"
#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/Thread.h>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>

namespace PlagC::Editor {
namespace {
class ModelValueCommand final : public Command
{
public:
    ModelValueCommand(std::string label, std::function<void()> apply, std::function<void()> revert)
        : label_(std::move(label)), apply_(std::move(apply)), revert_(std::move(revert))
    {
        if (!apply_ || !revert_)
            throw std::invalid_argument("Model value command requires apply and revert callbacks");
    }
    std::string_view Label() const noexcept override { return label_; }
    void Apply() override { apply_(); }
    void Revert() override { revert_(); }
private:
    std::string label_;
    std::function<void()> apply_;
    std::function<void()> revert_;
};

void TrimOverrides(ModelComponent& model)
{
    while (!model.materialOverrides.empty() && !model.materialOverrides.back())
        model.materialOverrides.pop_back();
}
} // namespace

void SceneDocument::SetModelCastShadows(Entity entity, bool castShadows)
{
    Thread::RequireMain();
    RequireStableEditState();
    const auto record = world_->Read(entity);
    if (!record.model) throw std::invalid_argument("Entity has no model component");
    if (record.model->castShadows == castShadows) return;
    const std::uint64_t id = PersistentID(entity);
    const ModelComponent before = *record.model;
    ModelComponent after = before;
    after.castShadows = castShadows;
    auto write = [this, id](const ModelComponent& value) {
        const Entity target = FindByPersistentID(id);
        if (!target) throw std::logic_error("Model property command refers to a missing entity");
        world_->SetModel(target, value);
    };
    history_.Execute(std::make_unique<ModelValueCommand>(
        "Edit model shadows", [write, after]() { write(after); }, [write, before]() { write(before); }));
    BumpRevision();
}

void SceneDocument::ReloadModelAsset(Entity entity, AssetHandle<ModelAsset> asset)
{
    Thread::RequireMain();
    RequireStableEditState();
    if (!asset) throw std::invalid_argument("Reloaded model asset is empty");
    const auto record = world_->Read(entity);
    if (!record.model || !record.model->asset) throw std::invalid_argument("Entity has no model component");
    const ModelComponent before = *record.model;
    if (before.asset->metadata.path != asset->metadata.path)
        throw std::invalid_argument("Model reload must preserve the asset path");
    if (before.asset.Shared() == asset.Shared()) return;
    const std::uint64_t id = PersistentID(entity);
    ModelComponent after = before;
    after.asset = std::move(asset);
    if (after.materialOverrides.size() > after.asset->materials.size())
        after.materialOverrides.resize(after.asset->materials.size());
    TrimOverrides(after);
    auto write = [this, id](const ModelComponent& value) {
        const Entity target = FindByPersistentID(id);
        if (!target) throw std::logic_error("Model reload command refers to a missing entity");
        world_->SetModel(target, value);
    };
    history_.Execute(std::make_unique<ModelValueCommand>(
        "Reload model asset", [write, after]() { write(after); }, [write, before]() { write(before); }));
    BumpRevision();
}

void SceneDocument::SetModelMaterial(Entity entity, std::size_t slot, AssetHandle<MaterialAsset> material)
{
    Thread::RequireMain();
    RequireStableEditState();
    if (!material) throw std::invalid_argument("Material override asset is empty");
    Validate(*material);
    if (material->assetPath.empty())
        throw std::invalid_argument("Material override must have a persistent .pgmaterial asset path");

    const auto record = world_->Read(entity);
    if (!record.model || !record.model->asset) throw std::invalid_argument("Entity has no model component");
    if (slot >= record.model->asset->materials.size())
        throw std::out_of_range("Material slot is outside the model material table");
    if (slot < record.model->materialOverrides.size() &&
        record.model->materialOverrides[slot].Shared() == material.Shared())
        return;

    const std::uint64_t id = PersistentID(entity);
    const ModelComponent before = *record.model;
    ModelComponent after = before;
    if (after.materialOverrides.size() <= slot)
        after.materialOverrides.resize(slot + 1);
    after.materialOverrides[slot] = std::move(material);
    TrimOverrides(after);

    auto write = [this, id](const ModelComponent& value) {
        const Entity target = FindByPersistentID(id);
        if (!target) throw std::logic_error("Material override command refers to a missing entity");
        world_->SetModel(target, value);
    };
    history_.Execute(std::make_unique<ModelValueCommand>(
        "Assign material", [write, after]() { write(after); }, [write, before]() { write(before); }));
    BumpRevision();
}

void SceneDocument::ClearModelMaterial(Entity entity, std::size_t slot)
{
    Thread::RequireMain();
    RequireStableEditState();
    const auto record = world_->Read(entity);
    if (!record.model || !record.model->asset) throw std::invalid_argument("Entity has no model component");
    if (slot >= record.model->asset->materials.size())
        throw std::out_of_range("Material slot is outside the model material table");
    if (slot >= record.model->materialOverrides.size() || !record.model->materialOverrides[slot])
        return;

    const std::uint64_t id = PersistentID(entity);
    const ModelComponent before = *record.model;
    ModelComponent after = before;
    after.materialOverrides[slot] = {};
    TrimOverrides(after);
    auto write = [this, id](const ModelComponent& value) {
        const Entity target = FindByPersistentID(id);
        if (!target) throw std::logic_error("Material override command refers to a missing entity");
        world_->SetModel(target, value);
    };
    history_.Execute(std::make_unique<ModelValueCommand>(
        "Clear material", [write, after]() { write(after); }, [write, before]() { write(before); }));
    BumpRevision();
}

} // namespace PlagC::Editor
