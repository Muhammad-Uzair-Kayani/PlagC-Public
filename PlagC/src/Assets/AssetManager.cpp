#include "pgcpch.h"
#include <PlagC/Assets/AssetManager.h>
#include <PlagC/Thread.h>
#include <array>
#include <optional>
#include <string_view>
namespace PlagC
{
namespace
{
std::atomic<std::uint64_t> nextGeneration{1};

std::uint64_t AssetPathID(std::string_view key)
{
    std::uint64_t id = 14695981039346656037ull;
    for (unsigned char c : key)
    {
        id ^= c;
        id *= 1099511628211ull;
    }
    return id ? id : 1;
}

std::optional<Primitive> BuiltinPrimitiveKind(std::string_view path)
{
    if (path == "Engine://Primitives/Cube.plasset") return Primitive::Cube;
    if (path == "Engine://Primitives/Plane.plasset") return Primitive::Plane;
    if (path == "Engine://Primitives/Sphere.plasset") return Primitive::Sphere;
    if (path == "Engine://Primitives/Cylinder.plasset") return Primitive::Cylinder;
    return std::nullopt;
}

std::size_t PrimitiveIndex(Primitive primitive)
{
    switch (primitive)
    {
    case Primitive::Cube: return 0;
    case Primitive::Plane: return 1;
    case Primitive::Sphere: return 2;
    case Primitive::Cylinder: return 3;
    }
    throw std::invalid_argument("unknown built-in primitive");
}

const char *PrimitiveName(Primitive primitive)
{
    switch (primitive)
    {
    case Primitive::Cube: return "Cube";
    case Primitive::Plane: return "Plane";
    case Primitive::Sphere: return "Sphere";
    case Primitive::Cylinder: return "Cylinder";
    }
    throw std::invalid_argument("unknown built-in primitive");
}

std::shared_ptr<const Asset> BuiltinPrimitiveAsset(const std::string &path, AssetType type)
{
    const auto primitive = BuiltinPrimitiveKind(path);
    if (!primitive)
        return {};
    if (type != AssetType::Model)
        throw std::invalid_argument("built-in primitive asset type mismatch");

    static std::array<std::shared_ptr<const ModelAsset>, 4> cache;
    auto &cached = cache[PrimitiveIndex(*primitive)];
    if (!cached)
    {
        auto asset = std::make_shared<ModelAsset>();
        asset->metadata.id = {AssetPathID(path)};
        asset->metadata.type = AssetType::Model;
        asset->metadata.path = path;
        asset->metadata.generation = 1;

        MeshAsset mesh;
        mesh.name = PrimitiveName(*primitive);
        mesh.geometry = MakePrimitive(*primitive);
        mesh.node = 0;
        mesh.material = 0;
        asset->meshes.push_back(std::move(mesh));

        MaterialAsset material;
        material.name = "Default";
        material.colour = {0.72f, 0.74f, 0.78f};
        material.roughness = 0.55f;
        asset->materials.push_back(std::move(material));

        AssetNode root;
        root.name = "Root";
        root.parent = -1;
        asset->skeleton.joints.push_back(std::move(root));

        const auto &geometry = asset->meshes.front().geometry;
        asset->metadata.bytes = geometry.vertices.size() * sizeof(Vertex) +
                                geometry.indices.size() * sizeof(std::uint32_t);
        Validate(*asset);
        cached = std::move(asset);
    }
    return cached;
}
}
struct AssetManager::Impl
{
    JobSystem &jobs;
    std::filesystem::path root;
    std::unordered_map<std::string, std::filesystem::path> mounts;
    JobGroup group;
    struct Entry
    {
        AssetInfo info;
        std::shared_ptr<const Asset> asset;
        std::shared_ptr<Asset> result;
        std::string failure;
        std::filesystem::path file;
        std::atomic<bool> done{false};
        bool submitted = false;
    };
    std::unordered_map<std::string, std::unique_ptr<Entry>> entries;
    Impl(JobSystem &j, std::filesystem::path p)
        : jobs(j), root(std::filesystem::weakly_canonical(p))
    {
    }
    std::pair<std::string, std::filesystem::path> Resolve(const std::string &path) const
    {
        if (path.empty())
            throw std::invalid_argument("asset path cannot be empty");

        const auto scheme = path.find("://");
        if (scheme != std::string::npos)
        {
            const std::string mountName = path.substr(0, scheme);
            const std::string relativeText = path.substr(scheme + 3);
            auto found = mounts.find(mountName);
            if (found == mounts.end())
                throw std::invalid_argument("unknown asset mount: " + mountName);

            std::filesystem::path relative(relativeText);
            if (relative.empty() || relative.is_absolute())
                throw std::invalid_argument("mounted asset path must be relative");

            const auto &mountRoot = found->second;
            auto full = std::filesystem::weakly_canonical(mountRoot / relative);
            auto relativeKey = full.lexically_relative(mountRoot);
            if (relativeKey.empty() || *relativeKey.begin() == "..")
                throw std::invalid_argument("asset path escapes mount: " + mountName);
            return {mountName + "://" + relativeKey.generic_string(), full};
        }

        std::filesystem::path relative(path);
        if (relative.is_absolute())
            throw std::invalid_argument("asset path must be relative to cooked root");
        auto full = std::filesystem::weakly_canonical(root / relative);
        auto key = full.lexically_relative(root);
        if (key.empty() || *key.begin() == "..")
            throw std::invalid_argument("asset path escapes cooked root");
        return {key.generic_string(), full};
    }
    static void Read(void *context)
    {
        auto &e = *static_cast<Entry *>(context);
        struct Complete
        {
            Entry &entry;
            ~Complete()
            {
                entry.done.store(true, std::memory_order_release);
            }
        } complete{e};
        try
        {
            e.result = ReadAsset(e.file);
            if (e.result->metadata.type != e.info.metadata.type)
                throw std::runtime_error("asset type mismatch");
            e.result->metadata.id = e.info.metadata.id;
            e.result->metadata.path = e.info.metadata.path;
            e.result->metadata.generation = e.info.metadata.generation;
        }
        catch (const std::exception &x)
        {
            e.result.reset();
            e.failure = x.what();
        }
        catch (...)
        {
            e.result.reset();
            e.failure = "unknown asset read failure";
        }
    }
};
AssetManager::AssetManager(JobSystem &jobs, std::filesystem::path root)
    : impl_(std::make_unique<Impl>(jobs, std::move(root)))
{
    Thread::RequireMain();
}
AssetManager::~AssetManager()
{
    Thread::RequireMain();
    impl_->jobs.Wait(impl_->group);
}
void AssetManager::Mount(std::string name, std::filesystem::path root)
{
    Thread::RequireMain();
    if (name.empty() || name.find("://") != std::string::npos)
        throw std::invalid_argument("invalid asset mount name");
    if (root.empty())
        throw std::invalid_argument("asset mount root cannot be empty");

    auto canonical = std::filesystem::weakly_canonical(root);
    auto [it, inserted] = impl_->mounts.emplace(std::move(name), std::move(canonical));
    if (!inserted)
        throw std::runtime_error("asset mount already exists: " + it->first);
}
void AssetManager::Unmount(const std::string &name)
{
    Thread::RequireMain();
    Wait();
    const std::string prefix = name + "://";
    for (const auto &[key, entry] : impl_->entries)
        if (key.rfind(prefix, 0) == 0)
            throw std::runtime_error("cannot unmount asset root while mounted assets are registered: " + name);
    impl_->mounts.erase(name);
}
bool AssetManager::HasMount(const std::string &name) const
{
    Thread::RequireMain();
    return impl_->mounts.find(name) != impl_->mounts.end();
}
std::filesystem::path AssetManager::MountedRoot(const std::string &name) const
{
    Thread::RequireMain();
    auto found = impl_->mounts.find(name);
    if (found == impl_->mounts.end())
        return {};
    return found->second;
}
void AssetManager::Request(const std::string &path, AssetType type)
{
    Thread::RequireMain();
    if (BuiltinPrimitiveAsset(path, type))
        return;

    auto &p = *impl_;
    auto [key, full] = p.Resolve(path);
    auto found = p.entries.find(key);
    if (found != p.entries.end())
    {
        if (found->second->info.metadata.type != type)
            throw std::invalid_argument("asset registered with another type");
        return;
    }
    if (p.entries.size() >= 4096)
        throw std::runtime_error("asset registry capacity reached");
    const std::uint64_t id = AssetPathID(key);
    for (const auto &[name, e] : p.entries)
        if (e->info.metadata.id.value == id)
            throw std::runtime_error("asset ID hash collision");
    auto e = std::make_unique<Impl::Entry>();
    e->info.metadata = {{id}, type, key, nextGeneration.fetch_add(1), 0};
    e->file = full;
    p.entries.emplace(key, std::move(e));
    Pump();
}
void AssetManager::Pump()
{
    Thread::RequireMain();
    auto &p = *impl_;
    for (auto &[key, e] : p.entries)
    {
        if (e->info.state == AssetState::Queued)
        {
            auto result = p.jobs.TrySubmit(p.group, Impl::Read, e.get());
            if (result == SubmitResult::Stopped)
            {
                e->info.state = AssetState::Failed;
                e->info.error = "job system stopped";
            }
            else if (result == SubmitResult::Accepted)
            {
                e->submitted = true;
                e->info.state = AssetState::Loading;
            }
        }
        if (e->info.state == AssetState::Loading && e->done.load(std::memory_order_acquire))
        {
            if (e->result)
            {
                e->asset = std::move(e->result);
                e->info.metadata = e->asset->metadata;
                e->info.state = AssetState::Ready;
                e->info.error.clear();
            }
            else
            {
                e->info.state = AssetState::Failed;
                e->info.error = e->failure;
            }
        }
    }
}
void AssetManager::Wait()
{
    Thread::RequireMain();
    for (;;)
    {
        Pump();
        impl_->jobs.Wait(impl_->group);
        Pump();
        bool pending = false;
        for (auto &[key, e] : impl_->entries)
            pending |= e->info.state == AssetState::Queued || e->info.state == AssetState::Loading;
        if (!pending)
            break;
    }
}
std::shared_ptr<const Asset> AssetManager::GetBase(const std::string &path, AssetType type) const
{
    Thread::RequireMain();
    if (auto builtin = BuiltinPrimitiveAsset(path, type))
        return builtin;

    auto key = impl_->Resolve(path).first;
    auto it = impl_->entries.find(key);
    if (it == impl_->entries.end())
        return {};
    auto &e = *it->second;
    if (e.info.metadata.type != type)
        throw std::invalid_argument("asset type mismatch");
    return e.asset;
}
std::shared_ptr<const Asset> AssetManager::LoadBase(const std::string &path, AssetType type)
{
    if (auto builtin = BuiltinPrimitiveAsset(path, type))
        return builtin;

    Request(path, type);
    Wait();
    auto value = GetBase(path, type);
    if (!value)
    {
        auto &e = *impl_->entries.at(impl_->Resolve(path).first);
        throw std::runtime_error(e.info.error);
    }
    return value;
}
void AssetManager::Reload(const std::string &path)
{
    Thread::RequireMain();
    if (BuiltinPrimitiveKind(path))
        return;

    Wait();
    auto &e = *impl_->entries.at(impl_->Resolve(path).first);
    e.done = false;
    e.submitted = false;
    e.failure.clear();
    e.info.metadata.generation = nextGeneration.fetch_add(1);
    e.info.state = AssetState::Queued;
    Pump();
}
void AssetManager::Unload(const std::string &path)
{
    Thread::RequireMain();
    if (BuiltinPrimitiveKind(path))
        return;

    Wait();
    impl_->entries.erase(impl_->Resolve(path).first);
}
std::size_t AssetManager::CollectUnused()
{
    Thread::RequireMain();
    Wait();
    return std::erase_if(impl_->entries, [](auto &pair) {
        auto &e = *pair.second;
        return e.info.state == AssetState::Ready && e.asset.use_count() == 1;
    });
}
std::vector<AssetInfo> AssetManager::Registry() const
{
    Thread::RequireMain();
    std::vector<AssetInfo> result;
    for (auto &[key, e] : impl_->entries)
        result.push_back(e->info);
    std::sort(result.begin(), result.end(),
              [](const auto &a, const auto &b) { return a.metadata.path < b.metadata.path; });
    return result;
}
} // namespace PlagC
