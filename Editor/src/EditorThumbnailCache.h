#pragma once

#include <PlagC/Renderer/Renderer.h>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace PlagC {
class Engine;
struct JobGroup;
}

namespace PlagC::Editor {

enum class ThumbnailKind : std::uint8_t {
    Model,
    Texture,
    Material
};

enum class ThumbnailState : std::uint8_t {
    Queued,
    Generating,
    Ready,
    Failed
};

struct ThumbnailView {
    ThumbnailState state = ThumbnailState::Queued;
    TextureHandle texture{};
    std::string_view error{};
};

// Renderer-neutral thumbnail pipeline. CPU preview pixels are generated on PlagC jobs;
// only finished RGBA buffers cross back to the main thread for UI-texture upload.
// This keeps preview generation backend-independent and avoids creating a second Ogre scene/root.
class EditorThumbnailCache {
public:
    static constexpr unsigned PreviewSize = 128;

    EditorThumbnailCache() = default;
    ~EditorThumbnailCache() = default;
    EditorThumbnailCache(const EditorThumbnailCache&) = delete;
    EditorThumbnailCache& operator=(const EditorThumbnailCache&) = delete;

    void Initialize();
    void Shutdown(Engine&);
    void Pump(Engine&);

    ThumbnailView Request(Engine&,
                          std::string key,
                          std::filesystem::path source,
                          ThumbnailKind kind,
                          std::uint64_t generation = 0);
    void Invalidate(Engine&, std::string_view key);
    void InvalidateAll(Engine&);

    std::size_t CachedCount() const noexcept { return entries_.size(); }
    std::size_t ActiveJobs() const noexcept { return activeJobs_; }

private:
    struct Task;
    struct Entry;

    static void RunTask(void* context);
    static std::uint64_t Fingerprint(const std::filesystem::path&, std::uint64_t generation);
    void LaunchQueued(Engine&);
    void Launch(Engine&, Entry&);
    void RetireReadyTask(Engine&, Entry&);
    void EvictIfNeeded(Engine&);

    std::unordered_map<std::string, std::unique_ptr<Entry>> entries_;
    std::size_t activeJobs_ = 0;
    std::uint64_t nextSerial_ = 1;
    std::uint64_t touchCounter_ = 1;
    bool initialized_ = false;
};

} // namespace PlagC::Editor
