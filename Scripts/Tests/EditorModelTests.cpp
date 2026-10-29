#include <PlagC/Editor/EditorModels.h>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>
using namespace PlagC;
using namespace PlagC::Editor;

int main() {
    Thread::InitializeMain();

    MainThreadMailbox mailbox(4);
    int committed = 0;
    std::thread producer([&] {
        assert(mailbox.Post([&] { committed = 7; }));
    });
    producer.join();
    assert(committed == 0 && mailbox.Pending() == 1);
    assert(mailbox.Drain(1) == 1 && committed == 7 && mailbox.Pending() == 0);
    GenerationGate gate;
    auto first = gate.Begin();
    auto second = gate.Begin();
    assert(!gate.Current(first) && gate.Current(second));
    gate.Invalidate();
    assert(!gate.Current(second));

    AssetBrowserModel assets;
    auto stale = assets.BeginRefresh();
    auto current = assets.BeginRefresh();

    std::vector<AssetBrowserEntry> staleEntries(1);
    staleEntries[0].path = "stale";
    assert(!assets.Publish(stale, std::move(staleEntries)));

    std::vector<AssetBrowserEntry> currentEntries(2);
    currentEntries[0].path = "z.plasset";
    currentEntries[1].path = "a.plasset";
    assert(assets.Publish(current, std::move(currentEntries)));
    auto published = assets.Snapshot();
    assert(published->size() == 2 && (*published)[0].path == "a.plasset");

    std::vector<AssetInfo> registry(2);
    registry[0].metadata.path = "Game://crate.plasset";
    registry[0].metadata.type = AssetType::Model;
    registry[0].metadata.bytes = 100;
    registry[0].state = AssetState::Ready;
    registry[1].metadata.path = "Game://broken.plasset";
    registry[1].metadata.type = AssetType::Audio;
    registry[1].state = AssetState::Failed;
    registry[1].error = "bad";
    auto runtimeEntries = AssetBrowserModel::FromRegistry(registry);
    assert(runtimeEntries.size() == 2);

    auto root = std::filesystem::temp_directory_path() /
        ("plagc-editor-assets-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root / "nested");
    std::ofstream(root / "model.glb").put('x');
    std::ofstream(root / "nested" / "image.png").put('x');
    const AssetRoot scanRoot{root, "Project"};
    auto discovered = AssetBrowserModel::ScanFilesystem(
        std::span<const AssetRoot>(&scanRoot, 1));
    assert(discovered.size() == 2);
    assert(discovered[0].path.find("Project://") == 0);
    std::filesystem::remove_all(root);

    std::vector<LogRecord> logs = {
        {1, Severity::Info, "t", "Engine", "Main", "1", "ready"},
        {2, Severity::Error, "t", "Renderer", "Main", "1", "shader failed"}
    };
    ConsoleFilter filter;
    filter.info = false;
    filter.search = "shader";
    auto filtered = FilterLogRecords(logs, filter);
    assert(filtered.size() == 1 && filtered[0].sequence == 2);

    FrameProfile frame;
    frame.fps = 144.0;
    frame.render.drawCalls = 20;
    JobStats jobs{10, 9, 1, 0};
    auto telemetry = MakeTelemetry(frame, jobs, registry, 42);
    assert(telemetry.entities == 42 && telemetry.assets == 2 &&
           telemetry.failedAssets == 1 && telemetry.frame.render.drawCalls == 20);

    ViewportState viewport;
    assert(viewport.Resize(1920, 1080));
    assert(viewport.Aspect() > 1.7f);
    viewport.input.viewportHovered = true;
    assert(viewport.input.AcceptCameraMouse());
    viewport.input.uiBlocksMouse = true;
    assert(!viewport.input.AcceptSelectionMouse());
    viewport.input.uiBlocksMouse = false;
    viewport.input.gizmoActive = true;
    assert(!viewport.input.AcceptCameraMouse());
    viewport.input.gizmoActive = false;
    viewport.input.viewportFocused = true;
    assert(viewport.input.AcceptKeyboard());
}
