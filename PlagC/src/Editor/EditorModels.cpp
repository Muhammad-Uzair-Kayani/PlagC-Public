#include "pgcpch.h"
#include <PlagC/Editor/EditorModels.h>
#include <algorithm>
#include <stdexcept>
#include <unordered_map>

namespace PlagC::Editor {

MainThreadMailbox::MainThreadMailbox(std::size_t capacity) : capacity_(capacity) {
    if (!capacity_)
        throw std::invalid_argument("Editor mailbox capacity must be positive");
}

bool MainThreadMailbox::Post(std::function<void()> task) {
    if (!task)
        throw std::invalid_argument("Cannot post an empty editor task");
    std::lock_guard lock(mutex_);
    if (tasks_.size() >= capacity_)
        return false;
    tasks_.push_back(std::move(task));
    return true;
}

std::size_t MainThreadMailbox::Drain(std::size_t maxTasks) {
    Thread::RequireMain();
    std::size_t executed = 0;
    while (executed < maxTasks) {
        std::function<void()> task;
        {
            std::lock_guard lock(mutex_);
            if (tasks_.empty())
                break;
            task = std::move(tasks_.front());
            tasks_.pop_front();
        }
        task();
        ++executed;
    }
    return executed;
}

std::size_t MainThreadMailbox::Pending() const {
    std::lock_guard lock(mutex_);
    return tasks_.size();
}

namespace {

std::string Lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string NormalizedPath(std::string value) {
    std::replace(value.begin(), value.end(), '\\', '/');
    return value;
}

EditorAssetKind KindFor(const std::filesystem::path& path) {
    const auto extension = Lower(path.extension().string());
    if (extension == ".plasset")
        return EditorAssetKind::Cooked;
    if (extension == ".gltf" || extension == ".glb" ||
        extension == ".fbx" || extension == ".obj" ||
        extension == ".dae")
        return EditorAssetKind::ModelSource;
    if (extension == ".wav" || extension == ".ogg" ||
        extension == ".flac")
        return EditorAssetKind::AudioSource;
    if (extension == ".png" || extension == ".jpg" ||
        extension == ".jpeg" || extension == ".dds" ||
        extension == ".tga" || extension == ".ktx" ||
        extension == ".ktx2")
        return EditorAssetKind::Texture;
    if (extension == ".glsl" || extension == ".hlsl" ||
        extension == ".vert" || extension == ".frag" ||
        extension == ".comp")
        return EditorAssetKind::Shader;
    if (extension == ".plscene" || extension == ".scene")
        return EditorAssetKind::Scene;
    if (extension == ".pgmaterial" || extension == ".material" || extension == ".mat")
        return EditorAssetKind::Material;
    return EditorAssetKind::Other;
}

std::string MountedPath(const AssetRoot& root,
                        const std::filesystem::path& relative) {
    const auto value = relative.generic_string();
    return root.mount.empty() ? value : root.mount + "://" + value;
}

std::optional<std::size_t> FindRegistryEntry(
    const std::vector<AssetBrowserEntry>& entries,
    const std::unordered_map<std::string, std::size_t>& byPath,
    std::string registryPath) {
    registryPath = NormalizedPath(std::move(registryPath));
    if (const auto exact = byPath.find(registryPath); exact != byPath.end())
        return exact->second;

    const std::string suffix = '/' + registryPath;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].kind != EditorAssetKind::Cooked)
            continue;
        const std::string candidate = NormalizedPath(entries[i].path);
        if (candidate == registryPath || candidate.ends_with(suffix))
            return i;
    }
    return std::nullopt;
}

bool SeverityEnabled(Severity severity, const ConsoleFilter& filter) {
    switch (severity) {
    case Severity::Trace: return filter.trace;
    case Severity::Debug: return filter.debug;
    case Severity::Info: return filter.info;
    case Severity::Warning: return filter.warning;
    case Severity::Error: return filter.error;
    case Severity::Critical: return filter.critical;
    }
    return false;
}

} // namespace

std::vector<AssetBrowserEntry>
AssetBrowserModel::ScanFilesystem(std::span<const AssetRoot> roots,
                                  std::size_t maxEntries) {
    if (!maxEntries)
        throw std::invalid_argument("Asset scan limit must be positive");

    std::vector<AssetBrowserEntry> result;
    result.reserve(std::min<std::size_t>(maxEntries, 4096));

    for (const auto& root : roots) {
        if (root.root.empty())
            continue;

        std::error_code rootError;
        const auto canonicalRoot = std::filesystem::weakly_canonical(root.root, rootError);
        if (rootError || !std::filesystem::is_directory(canonicalRoot, rootError))
            continue;

        std::filesystem::recursive_directory_iterator iterator(
            canonicalRoot,
            std::filesystem::directory_options::skip_permission_denied,
            rootError);
        const std::filesystem::recursive_directory_iterator end;

        while (!rootError && iterator != end) {
            const auto entry = *iterator;
            std::error_code entryError;

            if (entry.is_symlink(entryError)) {
                if (entry.is_directory(entryError))
                    iterator.disable_recursion_pending();
                iterator.increment(rootError);
                continue;
            }

            if (entry.is_regular_file(entryError)) {
                auto relative = std::filesystem::relative(entry.path(), canonicalRoot, entryError);
                if (!entryError) {
                    AssetBrowserEntry item;
                    item.path = MountedPath(root, relative);
                    item.kind = KindFor(entry.path());
                    item.bytes = entry.file_size(entryError);
                    if (entryError)
                        item.bytes = 0;
                    if (result.size() == maxEntries)
                        throw std::length_error("Editor asset scan limit exceeded");
                    result.push_back(std::move(item));
                }
            }

            iterator.increment(rootError);
        }
    }

    std::sort(result.begin(), result.end(),
              [](const AssetBrowserEntry& a, const AssetBrowserEntry& b) {
                  return a.path < b.path;
              });
    result.erase(std::unique(result.begin(), result.end(),
                             [](const AssetBrowserEntry& a, const AssetBrowserEntry& b) {
                                 return a.path == b.path;
                             }),
                 result.end());
    return result;
}

std::vector<AssetBrowserEntry>
AssetBrowserModel::FromRegistry(std::span<const AssetInfo> registry) {
    std::vector<AssetBrowserEntry> result;
    result.reserve(registry.size());

    for (const auto& info : registry) {
        AssetBrowserEntry item;
        item.path = info.metadata.path;
        item.kind = EditorAssetKind::Cooked;
        item.state = info.state;
        item.runtimeType = info.metadata.type;
        item.bytes = info.metadata.bytes;
        item.generation = info.metadata.generation;
        item.error = info.error;
        result.push_back(std::move(item));
    }

    std::sort(result.begin(), result.end(),
              [](const AssetBrowserEntry& a, const AssetBrowserEntry& b) {
                  return a.path < b.path;
              });
    return result;
}

void AssetBrowserModel::MergeRegistry(std::vector<AssetBrowserEntry>& entries,
                                      std::span<const AssetInfo> registry) {
    std::unordered_map<std::string, std::size_t> byPath;
    byPath.reserve(entries.size());
    for (std::size_t i = 0; i < entries.size(); ++i)
        byPath.emplace(NormalizedPath(entries[i].path), i);

    for (const auto& info : registry) {
        const auto match = FindRegistryEntry(entries, byPath, info.metadata.path);
        if (!match) {
            AssetBrowserEntry item;
            item.path = info.metadata.path;
            item.kind = EditorAssetKind::Cooked;
            item.state = info.state;
            item.runtimeType = info.metadata.type;
            item.bytes = info.metadata.bytes;
            item.generation = info.metadata.generation;
            item.error = info.error;
            byPath.emplace(NormalizedPath(item.path), entries.size());
            entries.push_back(std::move(item));
            continue;
        }

        auto& item = entries[*match];
        item.state = info.state;
        item.runtimeType = info.metadata.type;
        item.bytes = info.metadata.bytes;
        item.generation = info.metadata.generation;
        item.error = info.error;
    }

    std::sort(entries.begin(), entries.end(),
              [](const AssetBrowserEntry& a, const AssetBrowserEntry& b) {
                  return a.path < b.path;
              });
}

bool AssetBrowserModel::Publish(std::uint64_t generation,
                                std::vector<AssetBrowserEntry> entries) {
    if (!gate_.Current(generation))
        return false;

    std::sort(entries.begin(), entries.end(),
              [](const AssetBrowserEntry& a, const AssetBrowserEntry& b) {
                  return a.path < b.path;
              });

    auto snapshot =
        std::make_shared<const std::vector<AssetBrowserEntry>>(std::move(entries));

    std::unique_lock lock(mutex_);
    if (!gate_.Current(generation))
        return false;
    snapshot_ = std::move(snapshot);
    return true;
}

std::vector<LogRecord> FilterLogRecords(std::span<const LogRecord> records,
                                        const ConsoleFilter& filter) {
    std::vector<LogRecord> result;
    result.reserve(records.size());
    const auto search = Lower(filter.search);

    for (const auto& record : records) {
        if (!SeverityEnabled(record.severity, filter))
            continue;

        if (!search.empty()) {
            const auto haystack = Lower(record.category + "\n" + record.thread +
                                        "\n" + record.message);
            if (haystack.find(search) == std::string::npos)
                continue;
        }

        result.push_back(record);
    }
    return result;
}

EditorTelemetry MakeTelemetry(const FrameProfile& frame,
                              const JobStats& jobs,
                              std::span<const AssetInfo> assets,
                              std::size_t entityCount) {
    EditorTelemetry result;
    result.frame = frame;
    result.jobs = jobs;
    result.assets = assets.size();
    result.entities = entityCount;
    result.failedAssets =
        static_cast<std::size_t>(std::count_if(
            assets.begin(), assets.end(),
            [](const AssetInfo& info) { return info.state == AssetState::Failed; }));
    return result;
}

} // namespace PlagC::Editor
