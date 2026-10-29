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
    assert '#include <cstdlib>' in renderer
    assert 'Ogre::BT_DYNAMIC_PERSISTENT_COHERENT' in renderer
    assert 'buffer->map(0, mesh.pendingVertices.size())' in renderer
    assert 'pendingMeshUploads' in renderer
    assert 'mesh.uploadQueued' in renderer
    assert 'buffer->unmap(Ogre::UO_KEEP_PERSISTENT)' in renderer
    assert 'pendingTextureUploads' in renderer
    assert 'setStagingTextureMaxBudgetBytes' in renderer
    assert 'VulkanStagingBudget = 512ull * 1024ull * 1024ull' in renderer
    assert 'PC_VULKAN_FRAMES_IN_FLIGHT' in renderer
    assert 'parsed >= 2ul && parsed <= 4ul' in renderer
    assert 'options["VaoManager::mDynamicBufferMultiplier"] = std::to_string(framesInFlight)' in renderer
    assert 'GpuProfiler::AttachWorkspace(workspace)' in renderer
    assert 'GpuProfiler::AttachWorkspace(t.workspace)' in renderer
    assert 'data.vertices.size(), vertexType,' in renderer

    profiler = apply(
        work,
        ROOT / 'Build/Patches/GpuProfilerPerformance.lua',
        ROOT / 'PlagC/src/Renderer/GpuProfiler.cpp',
        'gpu_profiler')
    assert profiler.count('if((p.frame&3u)!=0u)return;') == 2

    ogre = ROOT / 'Vendor/ogre-next/RenderSystems/Vulkan/src'
    ogre_include = ROOT / 'Vendor/ogre-next/RenderSystems/Vulkan/include'

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

    allocator_stage = work / 'allocator_memory.cpp'
    allocator_stage.write_text(allocator)
    allocator_present = apply(
        work,
        ROOT / 'Build/Patches/VulkanPresentSemaphores.lua',
        allocator_stage,
        'allocator_present')
    assert 'mUsedPresentSemaphores' in allocator_present
    assert 'notifyPresentationWaitSemaphoreSubmitted' in allocator_present
    assert 'notifySwapchainIndexAcquired' in allocator_present

    vao_header = apply(
        work,
        ROOT / 'Build/Patches/VulkanVaoPresentHeader.lua',
        ogre_include / 'Vao/OgreVulkanVaoManager.h',
        'vao_header')
    assert 'FastArray<UsedSemaphore> mUsedPresentSemaphores;' in vao_header
    assert 'notifyPresentationWaitSemaphoreSubmitted' in vao_header
    assert 'notifySwapchainIndexAcquired' in vao_header

    window_header = apply(
        work,
        ROOT / 'Build/Patches/VulkanWindowPresentHeader.lua',
        ogre_include / 'OgreVulkanWindow.h',
        'window_header')
    assert 'uint32 _swapBuffers( VkSemaphore queueFinishSemaphore );' in window_header

    queue = apply(
        work,
        ROOT / 'Build/Patches/VulkanQueuePerformance.lua',
        ogre / 'OgreVulkanQueue.cpp',
        'queue')
    assert 'PlagCVulkanPresentScheduler.h' in queue
    assert 'sharedGraphicsPresentMutex' in queue
    assert 'queue-submit avg=' in queue
    assert '[PlagC Vulkan CPU] submit-tail avg=' in queue
    assert '[PlagC Vulkan CPU] new-command-buffer tail-wait avg=' in queue
    assert '[PlagC Vulkan CPU] end-command-buffer avg=' in queue
    assert 'mWindowsPendingSwap[windowIdx]->acquireNextSwapchain();' not in queue
    assert 'notifyPresentationWaitSemaphoreSubmitted' in queue

    window = apply(
        work,
        ROOT / 'Build/Patches/VulkanWindowPerformance.lua',
        ogre / 'OgreVulkanWindow.cpp',
        'window')
    assert 'PlagCVulkanPresentScheduler.h' in window
    assert 'PlagCVulkanPresentScheduler::get().enqueue' in window
    assert 'plagcConsumePresentResult' in window
    assert 'serializeWithGraphicsQueue' in window
    assert 'if( mSwapchainStatus == SwapchainReleased )' in window
    assert 'acquireNextSwapchain();' in window
    assert '[PlagC Vulkan WSI] acquire avg=' in window
    assert '[PlagC Vulkan WSI] present avg=' in window
    assert 'main-wait avg=' in window
    assert '[PlagC Vulkan CPU] present-enqueue avg=' in window
    assert 'PC_VULKAN_SWAPCHAIN_IMAGES' in window
    assert '[PlagC Vulkan WSI] swapchain images=' in window
    assert 'notifySwapchainIndexAcquired' in window
    assert 'uint32 VulkanWindowSwapChainBased::_swapBuffers' in window
    assert 'return currentSwapchainIdx;' in window
    # The blocking present call belongs only to the scheduler after generation.
    assert 'VkResult result = vkQueuePresentKHR( mDevice->mPresentQueue, &present );' not in window

    scheduler_h = (ROOT / 'Build/Patches/PlagCVulkanPresentScheduler.h').read_text()
    scheduler_cpp = (ROOT / 'Build/Patches/PlagCVulkanPresentScheduler.cpp').read_text()
    assert 'PlagCVulkanPresentCompletion' in scheduler_h
    assert 'sharedGraphicsPresentMutex' in scheduler_h
    assert 'std::thread mWorker' in scheduler_h
    assert 'vkQueuePresentKHR( request.queue, &presentInfo )' in scheduler_cpp
    assert 'mOutstanding' in scheduler_cpp
    assert 'mCompletionCv.wait' in scheduler_cpp

    device = apply(
        work,
        ROOT / 'Build/Patches/VulkanDeviceQueues.lua',
        ogre / 'OgreVulkanDevice.cpp',
        'device')
    assert 'gPlagCPresentQueueFamily' in device
    assert 'usedQueueCount[graphicsFamily]++' in device
    assert 'Dedicated present queue enabled' in device
    assert 'Dedicated present queue unavailable' in device

    build_vulkan = (ROOT / 'Build/Vulkan.lua').read_text()
    assert 'PlagCVulkanPresentScheduler.cpp' in build_vulkan
    assert 'PlagCVulkanPresentScheduler.h' in build_vulkan

print('Vulkan performance generation patches passed.')
