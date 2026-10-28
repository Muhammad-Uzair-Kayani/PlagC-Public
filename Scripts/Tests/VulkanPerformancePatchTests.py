#!/usr/bin/env python3
"""Regression checks for PlagC's generated Vulkan integration.

These tests deliberately exercise the exact Lua generation patches against the pinned Ogre
submodule and current engine renderer. They need no GPU or Vulkan runtime.
"""
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument('--lua', default='texlua')
args = parser.parse_args()

PATCH_RUNNER = r'''local patch = dofile(arg[1])
local f = assert(io.open(arg[2], "rb")); local original = f:read("*a"); f:close()
original = original:gsub("\r\n", "\n")
local result = patch(original)
assert(result == patch(original:gsub("\n", "\r\n")), "CRLF checkout must be deterministic")
assert(result == patch(original), "generation must be deterministic")
assert(not pcall(patch, result), "already-patched/changed input must fail closed")
f = assert(io.open(arg[3], "wb")); f:write(result); f:close()
'''


def apply(work: Path, patch: Path, source: Path, name: str) -> str:
    runner = work / f'{name}.lua'
    output = work / f'{name}.cpp'
    runner.write_text(PATCH_RUNNER)
    subprocess.run([args.lua, str(runner), str(patch), str(source), str(output)], check=True)
    return output.read_text()


with tempfile.TemporaryDirectory() as tmp:
    work = Path(tmp)
    renderer = apply(
        work,
        ROOT / 'Build/Patches/OgreRendererPerformance.lua',
        ROOT / 'PlagC/src/Renderer/OgreRenderer.cpp',
        'renderer')
    assert '#include <PlagC/GpuProfiler.h>' in renderer
    assert 'Ogre::BT_DYNAMIC_PERSISTENT_COHERENT' in renderer
    assert 'buffer->map(0, vertices.size())' in renderer
    assert 'buffer->unmap(Ogre::UO_KEEP_PERSISTENT)' in renderer
    assert 'pendingTextureUploads' in renderer
    assert 'setStagingTextureMaxBudgetBytes' in renderer
    assert 'VulkanStagingBudget = 512ull * 1024ull * 1024ull' in renderer
    assert 'options["VaoManager::mDynamicBufferMultiplier"] = "4"' in renderer
    assert 'GpuProfiler::AttachWorkspace(workspace)' in renderer
    assert 'GpuProfiler::AttachWorkspace(t.workspace)' in renderer
    assert 'dynamic ? Ogre::BT_DEFAULT : Ogre::BT_IMMUTABLE' not in renderer

    ogre = ROOT / 'Vendor/ogre-next/RenderSystems/Vulkan/src'
    render_system = apply(
        work,
        ROOT / 'Build/Patches/VulkanDeviceExtensions.lua',
        ogre / 'OgreVulkanRenderSystem.cpp',
        'render_system')
    assert 'VK_EXT_MEMORY_BUDGET_EXTENSION_NAME' in render_system
    assert 'deviceExtensions.push_back( VK_EXT_MEMORY_BUDGET_EXTENSION_NAME )' in render_system

    allocator = apply(
        work,
        ROOT / 'Build/Patches/VulkanMemory.lua',
        ogre / 'Vao/OgreVulkanVaoManager.cpp',
        'allocator')
    assert 'VkPhysicalDeviceMemoryBudgetPropertiesEXT' in allocator
    assert 'availableHeapMemory' in allocator
    assert '8u * 1024u * 1024u' in allocator
    assert 'VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT' in allocator
    assert 'sizeBytes <= availableHeapMemory[heapIdx]' in allocator
    assert 'kind=' in allocator
    assert 'dedicated-texture' in allocator
    assert 'buffer-pool' in allocator
    assert 'requested=' in allocator and 'reserved=' in allocator

    queue = apply(
        work,
        ROOT / 'Build/Patches/VulkanQueuePerformance.lua',
        ogre / 'OgreVulkanQueue.cpp',
        'queue')
    assert 'do not acquire the next image immediately after presenting' in queue
    assert 'mWindowsPendingSwap[windowIdx]->acquireNextSwapchain();' not in queue

    window = apply(
        work,
        ROOT / 'Build/Patches/VulkanWindowPerformance.lua',
        ogre / 'OgreVulkanWindow.cpp',
        'window')
    assert 'if( mSwapchainStatus == SwapchainReleased )' in window
    assert 'acquireNextSwapchain();' in window
    assert 'acquire on demand' in window
    assert '[PlagC Vulkan WSI] acquire avg=' in window
    assert '[PlagC Vulkan WSI] present avg=' in window

    device = apply(
        work,
        ROOT / 'Build/Patches/VulkanDeviceQueues.lua',
        ogre / 'OgreVulkanDevice.cpp',
        'device')
    assert 'gPlagCPresentQueueFamily' in device
    assert 'usedQueueCount[graphicsFamily]++' in device
    assert 'Dedicated present queue enabled' in device
    assert 'Dedicated present queue unavailable' in device

print('Vulkan performance generation patches passed.')
