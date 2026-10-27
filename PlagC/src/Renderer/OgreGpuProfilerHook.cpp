#include "pgcpch.h"
#include <PlagC/OgreGpuProfilerHook.h>
#include <PlagC/GpuProfiler.h>

namespace PlagC
{
void AttachGpuProfilerToOgreWorkspace(void *workspace)
{
#ifdef PC_ENABLE_PROFILING
    GpuProfiler::AttachWorkspace(workspace);
#else
    (void)workspace;
#endif
}
} // namespace PlagC
