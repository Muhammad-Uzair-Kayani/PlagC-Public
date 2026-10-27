#pragma once
#include <PlagC/Assets/Asset.h>
#include <PlagC/Jobs.h>
#include <future>
namespace PlagC
{
enum class AssetState
{
    Queued,
    Loading,
    Ready,
    Failed
};
struct AssetInfo
{
    AssetMetadata metadata;
    AssetState state = AssetState::Queued;
    std::string error;
};
// Owner-thread registry/cache. CPU reads execute on Jobs; renderer/audio finalization is separate.
// Handles hold immutable snapshots across unload/reload and manager destruction.
//
// Legacy relative paths resolve against the default cooked root. Games may add named mounts and
// address them as "MountName://relative/path.plasset" without changing the engine-wide default root.
class AssetManager
{
  public:
    AssetManager(JobSystem &, std::filesystem::path root = "Assets/Cooked");
    ~AssetManager();
    AssetManager(const AssetManager &) = delete;
    AssetManager &operator=(const AssetManager &) = delete;

    void Mount(std::string name, std::filesystem::path root);
    void Unmount(const std::string &name);
    bool HasMount(const std::string &name) const;
    std::filesystem::path MountedRoot(const std::string &name) const;

    template <class T> AssetHandle<T> Load(const std::string &path)
    {
        auto a = LoadBase(path, T::Type);
        return AssetHandle<T>(std::dynamic_pointer_cast<const T>(a));
    }
    void Request(const std::string &, AssetType);
    template <class T> AssetHandle<T> Get(const std::string &path) const
    {
        return AssetHandle<T>(std::dynamic_pointer_cast<const T>(GetBase(path, T::Type)));
    }
    void Pump();
    void Wait();
    void Reload(const std::string &);
    void Unload(const std::string &);
    std::vector<AssetInfo> Registry() const;
    std::size_t CollectUnused();

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::shared_ptr<const Asset> LoadBase(const std::string &, AssetType);
    std::shared_ptr<const Asset> GetBase(const std::string &, AssetType) const;
};
} // namespace PlagC
