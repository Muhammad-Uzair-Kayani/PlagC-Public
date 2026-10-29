#pragma once

#include <PlagC/Application.h>
#include <PlagC/Editor/EditorModels.h>
#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/Thread.h>
#include <vector>

namespace PlagC::Editor {

struct EngineEditorSnapshot {
    EditorTelemetry telemetry{};
    std::vector<LogRecord> logs;
    std::vector<AssetInfo> assets;
};

// Main-thread bridge used by the immediate-mode UI. Expensive/background work
// (filesystem discovery, thumbnails, imports) stays outside this capture and
// publishes through generation-checked models instead.
inline EngineEditorSnapshot CaptureEngineEditorSnapshot(
    Engine& engine,
    const SceneDocument& document,
    const ConsoleFilter& filter = {}) {
    Thread::RequireMain();

    EngineEditorSnapshot result;
    result.assets = engine.Assets().Registry();
    result.logs = FilterLogRecords(engine.Logger().Snapshot(), filter);
    result.telemetry = MakeTelemetry(
        engine.Profile().Latest(),
        engine.Jobs().Stats(),
        result.assets,
        document.Scene().Size());
    return result;
}

} // namespace PlagC::Editor
