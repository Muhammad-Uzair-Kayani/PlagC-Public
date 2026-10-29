#pragma once

#include <PlagC/Assets/AssetManager.h>
#include <PlagC/Jobs.h>
#include <PlagC/Log.h>
#include <PlagC/Profiler.h>
#include <PlagC/Thread.h>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <deque>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace PlagC::Editor {

// Low-frequency editor gateway for worker-produced results that must commit on
// the engine/main thread. Tasks execute without holding the queue mutex; if one
// task throws, later tasks stay queued for the next drain.
class MainThreadMailbox {
public:
    explicit MainThreadMailbox(std::size_t capacity = 4096);

    bool Post(std::function<void()> task);
    std::size_t Drain(std::size_t maxTasks = 4096);
    std::size_t Pending() const;

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::deque<std::function<void()>> tasks_;
};

// Monotonic request epochs are used by worker-produced editor data. A job may
// finish after a newer request; Current() prevents that stale result replacing
// the newest UI snapshot.
class GenerationGate {
public:
    std::uint64_t Begin() noexcept {
        return generation_.fetch_add(1, std::memory_order_acq_rel) + 1;
    }

    void Invalidate() noexcept {
        generation_.fetch_add(1, std::memory_order_acq_rel);
    }

    bool Current(std::uint64_t generation) const noexcept {
        return generation != 0 &&
               generation_.load(std::memory_order_acquire) == generation;
    }

    std::uint64_t Value() const noexcept {
        return generation_.load(std::memory_order_acquire);
    }

private:
    std::atomic<std::uint64_t> generation_{0};
};

enum class EditorAssetKind {
    Cooked,
    ModelSource,
    AudioSource,
    Texture,
    Shader,
    Scene,
    Material,
    Other
};

struct AssetRoot {
    std::filesystem::path root;
    std::string mount;
};

struct AssetBrowserEntry {
    std::string path;
    EditorAssetKind kind = EditorAssetKind::Other;
    std::optional<AssetState> state;
    std::optional<AssetType> runtimeType;
    std::uint64_t bytes = 0;
    std::uint64_t generation = 0;
    std::string error;
};

class AssetBrowserModel {
public:
    std::uint64_t BeginRefresh() noexcept { return gate_.Begin(); }
    void Invalidate() noexcept { gate_.Invalidate(); }

    // Worker-safe filesystem discovery. No AssetManager or renderer APIs are
    // touched here. Results are published separately using the request epoch.
    static std::vector<AssetBrowserEntry>
    ScanFilesystem(std::span<const AssetRoot> roots, std::size_t maxEntries = 100000);

    // Owner-thread AssetManager state can be merged into discovered files or
    // used alone for a loaded-assets view.
    static std::vector<AssetBrowserEntry>
    FromRegistry(std::span<const AssetInfo> registry);

    static void MergeRegistry(std::vector<AssetBrowserEntry>& entries,
                              std::span<const AssetInfo> registry);

    bool Publish(std::uint64_t generation, std::vector<AssetBrowserEntry> entries);

    std::shared_ptr<const std::vector<AssetBrowserEntry>> Snapshot() const {
        std::shared_lock lock(mutex_);
        return snapshot_;
    }

private:
    GenerationGate gate_;
    mutable std::shared_mutex mutex_;
    std::shared_ptr<const std::vector<AssetBrowserEntry>> snapshot_ =
        std::make_shared<const std::vector<AssetBrowserEntry>>();
};

struct ConsoleFilter {
    bool trace = true;
    bool debug = true;
    bool info = true;
    bool warning = true;
    bool error = true;
    bool critical = true;
    std::string search;
};

std::vector<LogRecord> FilterLogRecords(std::span<const LogRecord>,
                                        const ConsoleFilter& = {});

struct EditorTelemetry {
    FrameProfile frame{};
    JobStats jobs{};
    std::size_t assets = 0;
    std::size_t failedAssets = 0;
    std::size_t entities = 0;
};

EditorTelemetry MakeTelemetry(const FrameProfile&,
                              const JobStats&,
                              std::span<const AssetInfo>,
                              std::size_t entityCount);

enum class GizmoMode {
    Translate,
    Rotate,
    Scale
};

struct InputCaptureState {
    // The UI adapter decides whether editor chrome/modal widgets block scene
    // input. Do not pass Dear ImGui's global WantCaptureMouse blindly because
    // the viewport itself is an ImGui window.
    bool uiBlocksMouse = false;
    bool uiBlocksKeyboard = false;
    bool viewportHovered = false;
    bool viewportFocused = false;
    bool gizmoActive = false;

    bool AcceptCameraMouse() const noexcept {
        return viewportHovered && !uiBlocksMouse && !gizmoActive;
    }

    bool AcceptSelectionMouse() const noexcept {
        return viewportHovered && !uiBlocksMouse && !gizmoActive;
    }

    bool AcceptKeyboard() const noexcept {
        return viewportFocused && !uiBlocksKeyboard;
    }
};

struct ViewportState {
    unsigned width = 1;
    unsigned height = 1;
    GizmoMode gizmo = GizmoMode::Translate;
    bool gridVisible = true;
    bool snapEnabled = false;
    float translationSnap = 0.5f;
    float rotationSnapDegrees = 15.0f;
    float scaleSnap = 0.1f;
    InputCaptureState input{};

    bool Resize(unsigned newWidth, unsigned newHeight) noexcept {
        newWidth = newWidth ? newWidth : 1;
        newHeight = newHeight ? newHeight : 1;
        if (newWidth == width && newHeight == height)
            return false;
        width = newWidth;
        height = newHeight;
        return true;
    }

    float Aspect() const noexcept {
        return static_cast<float>(width) / static_cast<float>(height);
    }
};

struct PickRequest {
    // Pixel coordinates in the editor viewport, not the OS window.
    float x = 0.0f;
    float y = 0.0f;
    unsigned width = 1;
    unsigned height = 1;
    CameraDesc camera{};
};

struct PickResult {
    std::uint64_t persistentID = 0;
    float distance = 0.0f;
};

class PickingProvider {
public:
    virtual ~PickingProvider() = default;
    virtual std::optional<PickResult> Pick(const PickRequest&) = 0;
};

} // namespace PlagC::Editor
