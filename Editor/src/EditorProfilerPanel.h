#pragma once

namespace PlagC {
class Engine;
}

namespace PlagC::Editor {
class EditorSession;

// Appends the Milestone 20 profiler surface to the Statistics window that is
// owned by EditorLayer. The panel owns presentation history only; engine
// profiling data remains owned by Profiler, JobSystem, Renderer and clients.
void DrawEditorProfilerPanel(Engine&, EditorSession&);

} // namespace PlagC::Editor
