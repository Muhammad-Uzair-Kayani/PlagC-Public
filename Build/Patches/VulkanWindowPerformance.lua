-- Defer WSI image acquisition until the next frame actually renders to the window.
-- Ogre 3.0 acquires immediately after vkQueuePresentKHR, which makes renderOneFrame absorb
-- presentation-engine backpressure. PlagC has ~2-3 ms of game work between frames; delaying
-- acquisition lets that work overlap WSI progress without changing Vulkan ordering.
return function(source)
    source = source:gsub("\r\n", "\n")
    local function replace(old, new)
        local first, last = source:find(old, 1, true)
        assert(first, 'Pinned Ogre VulkanWindow changed: ' .. old)
        assert(not source:find(old, last + 1, true), 'Ambiguous VulkanWindow performance patch: ' .. old)
        source = source:sub(1, first - 1) .. new .. source:sub(last + 1)
    end

    replace('#include "vulkan/vulkan_core.h"', [[#include "vulkan/vulkan_core.h"
#include "OgreStringConverter.h"
#include <algorithm>
#include <chrono>]])

    replace([[        uint32 swapchainIdx = 0u;
        VkResult result = vkAcquireNextImageKHR( mDevice->mDevice, mSwapchain, UINT64_MAX,
                                                 mSwapchainSemaphore, VK_NULL_HANDLE, &swapchainIdx );]], [[        uint32 swapchainIdx = 0u;
        const auto plagcAcquireStart = std::chrono::steady_clock::now();
        VkResult result = vkAcquireNextImageKHR( mDevice->mDevice, mSwapchain, UINT64_MAX,
                                                 mSwapchainSemaphore, VK_NULL_HANDLE, &swapchainIdx );
        const double plagcAcquireMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - plagcAcquireStart ).count();
        static double plagcAcquireTotalMs = 0.0;
        static double plagcAcquireMaxMs = 0.0;
        static uint32 plagcAcquireSamples = 0u;
        plagcAcquireTotalMs += plagcAcquireMs;
        plagcAcquireMaxMs = std::max( plagcAcquireMaxMs, plagcAcquireMs );
        if( ++plagcAcquireSamples == 240u )
        {
            LogManager::getSingleton().logMessage(
                "[PlagC Vulkan WSI] acquire avg=" +
                StringConverter::toString( plagcAcquireTotalMs / double( plagcAcquireSamples ) ) +
                " ms max=" + StringConverter::toString( plagcAcquireMaxMs ) + " ms" );
            plagcAcquireSamples = 0u;
            plagcAcquireTotalMs = 0.0;
            plagcAcquireMaxMs = 0.0;
        }
        if( result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR )
            vaoManager->notifySwapchainIndexAcquired( swapchainIdx );]])

    replace([[    VkSemaphore VulkanWindowSwapChainBased::getImageAcquiredSemaphore()
    {
        OGRE_ASSERT_LOW( mSwapchainStatus != SwapchainReleased );]], [[    VkSemaphore VulkanWindowSwapChainBased::getImageAcquiredSemaphore()
    {
        // PlagC: acquire on demand. The previous frame queued presentation before returning
        // to gameplay, so by the time rendering reaches this point the presentation engine
        // has had useful CPU time to release an image. This removes an unnecessary immediate
        // present -> acquire serialization point from the end of every renderOneFrame.
        if( mSwapchainStatus == SwapchainReleased )
            acquireNextSwapchain();
        OGRE_ASSERT_LOW( mSwapchainStatus != SwapchainReleased );]])

    replace([[        if( mSwapchainStatus == SwapchainAcquired || mSwapchainStatus == SwapchainPendingSwap )
        {
            // Ogre never rendered to this window. There's nothing to present.]], [[        if( mSwapchainStatus == SwapchainReleased || mSwapchainStatus == SwapchainAcquired ||
            mSwapchainStatus == SwapchainPendingSwap )
        {
            // Ogre never rendered to this window. With lazy acquisition, Released is also a
            // valid idle/minimized state and there is deliberately nothing to present.]])

    replace('    void VulkanWindowSwapChainBased::_swapBuffers( VkSemaphore queueFinishSemaphore )',
            '    uint32 VulkanWindowSwapChainBased::_swapBuffers( VkSemaphore queueFinishSemaphore )')

    replace([[        VkResult result = vkQueuePresentKHR( mDevice->mPresentQueue, &present );]], [[        const auto plagcPresentStart = std::chrono::steady_clock::now();
        VkResult result = vkQueuePresentKHR( mDevice->mPresentQueue, &present );
        const double plagcPresentMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - plagcPresentStart ).count();
        static double plagcPresentTotalMs = 0.0;
        static double plagcPresentMaxMs = 0.0;
        static uint32 plagcPresentSamples = 0u;
        plagcPresentTotalMs += plagcPresentMs;
        plagcPresentMaxMs = std::max( plagcPresentMaxMs, plagcPresentMs );
        if( ++plagcPresentSamples == 240u )
        {
            LogManager::getSingleton().logMessage(
                "[PlagC Vulkan WSI] present avg=" +
                StringConverter::toString( plagcPresentTotalMs / double( plagcPresentSamples ) ) +
                " ms max=" + StringConverter::toString( plagcPresentMaxMs ) + " ms" );
            plagcPresentSamples = 0u;
            plagcPresentTotalMs = 0.0;
            plagcPresentMaxMs = 0.0;
        }]])

    replace([[        mSwapchainStatus = SwapchainReleased;
    }
    //-------------------------------------------------------------------------
    void VulkanWindowSwapChainBased::getCustomAttribute]], [[        mSwapchainStatus = SwapchainReleased;
        return currentSwapchainIdx;
    }
    //-------------------------------------------------------------------------
    void VulkanWindowSwapChainBased::getCustomAttribute]])

    return source
end
