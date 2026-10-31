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

class GenerationGate {
public:
    std::uint64_t Begin() noexcept { return generation_.fetch_add(1, std::memory_order_acq_rel) + 1; }
    void Invalidate() noexcept { generation_.fetch_add(1, std::memory_order_acq_rel); }
    bool Current(std::uint64_t generation) const noexcept {
        return generation != 0 && generation_.load(std::memory_order_acquire) == generation;
    }
    std::uint64_t Value() const noexcept { return generation_.load(std::memory_order_acquire); }
private:
    std::atomic<std::uint64_t> generation_{0};
};

enum class EditorAssetKind {
    Cooked, ModelSource, AudioSource, Texture, Shader, Scene, Material, Other
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
    static std::vector<AssetBrowserEntry>
    ScanFilesystem(std::span<const AssetRoot> roots, std::size_t maxEntries = 100000);
    static std::vector<AssetBrowserEntry> FromRegistry(std::span<const AssetInfo> registry);
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
    bool trace = true, debug = true, info = true, warning = true, error = true, critical = true;
    std::string search;
};

std::vector<LogRecord> FilterLogRecords(std::span<const LogRecord>, const ConsoleFilter& = {});

struct EditorTelemetry {
    FrameProfile frame{};
    JobStats jobs{};
    std::size_t assets = 0, failedAssets = 0, entities = 0;
};

EditorTelemetry MakeTelemetry(const FrameProfile&, const JobStats&, std::span<const AssetInfo>,
                              std::size_t entityCount);

enum class GizmoMode { Translate, Rotate, Scale };
enum class TransformSpace { World, Local };
enum class TransformPivot { Pivot, Center };

struct InputCaptureState {
    bool uiBlocksMouse = false;
    bool uiBlocksKeyboard = false;
    bool viewportHovered = false;
    bool viewportFocused = false;
    bool gizmoActive = false;
    bool AcceptCameraMouse() const noexcept { return viewportHovered && !uiBlocksMouse && !gizmoActive; }
    bool AcceptSelectionMouse() const noexcept { return viewportHovered && !uiBlocksMouse && !gizmoActive; }
    bool AcceptKeyboard() const noexcept { return viewportFocused && !uiBlocksKeyboard; }
};

struct ViewportState {
    unsigned width = 1;
    unsigned height = 1;
    GizmoMode gizmo = GizmoMode::Translate;
    TransformSpace transformSpace = TransformSpace::World;
    TransformPivot pivot = TransformPivot::Pivot;
    bool gridVisible = true;
    bool gizmosVisible = true;
    bool previewLighting = true;
    bool selectionFeedback = true;
    bool orthographic = false;
    bool snapEnabled = false;
    float orthographicHeight = 12.0f;
    float translationSnap = 0.5f;
    float rotationSnapDegrees = 15.0f;
    float scaleSnap = 0.1f;
    float cameraSpeed = 8.0f;
    InputCaptureState input{};

    bool Resize(unsigned newWidth, unsigned newHeight) noexcept {
        newWidth = newWidth ? newWidth : 1;
        newHeight = newHeight ? newHeight : 1;
        if (newWidth == width && newHeight == height) return false;
        width = newWidth;
        height = newHeight;
        return true;
    }
    float Aspect() const noexcept { return static_cast<float>(width) / static_cast<float>(height); }
};

struct PickRequest {
    float x = 0.0f;
    float y = 0.0f;
    unsigned width = 1;
    unsigned height = 1;
    CameraDesc camera{};
    bool orthographic = false;
    float orthographicHeight = 12.0f;
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
