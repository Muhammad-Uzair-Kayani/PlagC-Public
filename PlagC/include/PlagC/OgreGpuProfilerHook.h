#pragma once

namespace PlagC
{
class Renderer;

// Kept separate from the public Renderer API so Ogre-specific profiling remains
// an implementation detail. The renderer calls this after creating/recreating
// its main compositor workspace.
void AttachGpuProfilerToOgreWorkspace(void *workspace);
} // namespace PlagC
