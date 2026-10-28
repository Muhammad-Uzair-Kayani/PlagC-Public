-- Pipeline WSI presentation and acquisition across game frames.
-- Keep replacements fail-closed, but anchor them to Vulkan statements rather than cosmetic
-- continuation-line whitespace in the pinned Ogre source.
return function(source)
    source = source:gsub("\r\n", "\n")

    local function replace(old, new)
        local first, last = source:find(old, 1, true)
        assert(first, 'Pinned Ogre VulkanWindow changed: ' .. old)
        assert(not source:find(old, last + 1, true),
               'Ambiguous VulkanWindow performance patch: ' .. old)
        source = source:sub(1, first - 1) .. new .. source:sub(last + 1)
    end

    replace('#include "vulkan/vulkan_core.h"', [[#include "vulkan/vulkan_core.h"
#include "OgreStringConverter.h"
#include "PlagCVulkanPresentScheduler.h"
#include <algorithm>
#include <chrono>
#include <cstdlib>]])

    replace([[namespace Ogre
{]], [[namespace Ogre
{
    namespace
    {
        bool plagcConsumePresentResult( VkSwapchainKHR swapchain )
        {
            const PlagCVulkanPresentCompletion completion =
                PlagCVulkanPresentScheduler::get().waitFor( swapchain );
            if( !completion.valid )
                return false;

            static double presentHostTotalMs = 0.0;
            static double presentHostMaxMs = 0.0;
            static double presentQueueDelayTotalMs = 0.0;
            static double presentMainWaitTotalMs = 0.0;
            static double presentMainWaitMaxMs = 0.0;
            static uint32 presentSamples = 0u;

            presentHostTotalMs += completion.hostMs;
            presentHostMaxMs = std::max( presentHostMaxMs, completion.hostMs );
            presentQueueDelayTotalMs += completion.queueDelayMs;
            presentMainWaitTotalMs += completion.mainWaitMs;
            presentMainWaitMaxMs = std::max( presentMainWaitMaxMs, completion.mainWaitMs );

            if( ++presentSamples == 240u )
            {
                LogManager::getSingleton().logMessage(
                    "[PlagC Vulkan WSI] present avg=" +
                    StringConverter::toString( presentHostTotalMs / double( presentSamples ) ) +
                    " ms max=" + StringConverter::toString( presentHostMaxMs ) +
                    " ms queue-delay avg=" +
                    StringConverter::toString( presentQueueDelayTotalMs / double( presentSamples ) ) +
                    " ms main-wait avg=" +
                    StringConverter::toString( presentMainWaitTotalMs / double( presentSamples ) ) +
                    " ms max=" + StringConverter::toString( presentMainWaitMaxMs ) + " ms" );
                presentSamples = 0u;
                presentHostTotalMs = 0.0;
                presentHostMaxMs = 0.0;
                presentQueueDelayTotalMs = 0.0;
                presentMainWaitTotalMs = 0.0;
                presentMainWaitMaxMs = 0.0;
            }

            if( completion.result != VK_SUCCESS && completion.result != VK_SUBOPTIMAL_KHR &&
                completion.result != VK_ERROR_OUT_OF_DATE_KHR )
            {
                LogManager::getSingleton().logMessage(
                    "[VulkanWindowSwapChainBased::swapBuffers] async vkQueuePresentKHR: error "
                    "presenting VkResult = " + vkResultToString( completion.result ) );
            }

            return completion.result == VK_SUBOPTIMAL_KHR ||
                   completion.result == VK_ERROR_OUT_OF_DATE_KHR;
        }
    }]])

    replace([[        uint32 minImageCount =
            std::max<uint32>( surfaceCaps.minImageCount, vaoManager->getDynamicBufferMultiplier() ) + 1;
        if( surfaceCaps.maxImageCount != 0u )
            minImageCount = std::min<uint32>( minImageCount, surfaceCaps.maxImageCount );]], [[        // Keep Ogre's safe default, with a bounded runtime override for WSI experiments.
        uint32 requestedImageCount = 0u;
        if( const char *requestedImages = std::getenv( "PC_VULKAN_SWAPCHAIN_IMAGES" ) )
        {
            char *parseEnd = 0;
            const unsigned long parsed = std::strtoul( requestedImages, &parseEnd, 10 );
            if( parseEnd != requestedImages && *parseEnd == '\0' && parsed >= 2ul && parsed <= 8ul )
                requestedImageCount = static_cast<uint32>( parsed );
        }

        uint32 minImageCount =
            std::max<uint32>( surfaceCaps.minImageCount, vaoManager->getDynamicBufferMultiplier() ) + 1;
        if( requestedImageCount )
        {
            minImageCount = std::max<uint32>(
                surfaceCaps.minImageCount,
                std::max<uint32>( vaoManager->getDynamicBufferMultiplier(), requestedImageCount ) );
        }
        if( surfaceCaps.maxImageCount != 0u )
            minImageCount = std::min<uint32>( minImageCount, surfaceCaps.maxImageCount );]])

    -- Anchor on the second result check plus acquire. This deliberately ignores the indentation
    -- of mSwapchainImages.begin(), which is cosmetic and caused the previous generation failure.
    replace([[        checkVkResult( result, "vkGetSwapchainImagesKHR" );

        acquireNextSwapchain();]], [[        checkVkResult( result, "vkGetSwapchainImagesKHR" );
        LogManager::getSingleton().logMessage(
            "[PlagC Vulkan WSI] swapchain images=" + StringConverter::toString( numSwapchainImages ) +
            " requested=" + StringConverter::toString( requestedImageCount ) +
            " frames-in-flight=" +
            StringConverter::toString( vaoManager->getDynamicBufferMultiplier() ) +
            " surface-min=" + StringConverter::toString( surfaceCaps.minImageCount ) +
            " surface-max=" + StringConverter::toString( surfaceCaps.maxImageCount ) );

        acquireNextSwapchain();]])

    replace([[    void VulkanWindowSwapChainBased::destroySwapchain()
    {
        mDevice->mRenderSystem->notifySwapchainDestroyed( this );]], [[    void VulkanWindowSwapChainBased::destroySwapchain()
    {
        // Never destroy a swapchain while the presentation worker is still using it.
        if( mSwapchain )
            (void)plagcConsumePresentResult( mSwapchain );
        mDevice->mRenderSystem->notifySwapchainDestroyed( this );]])

    replace([[        VulkanVaoManager *vaoManager = mDevice->mVaoManager;

        mSwapchainSemaphore = vaoManager->getAvailableSemaphore();]], [[        // Join the previous async present only when this swapchain is needed again.
        if( mSwapchain )
            mSuboptimal = plagcConsumePresentResult( mSwapchain ) || mSuboptimal;

        VulkanVaoManager *vaoManager = mDevice->mVaoManager;

        mSwapchainSemaphore = vaoManager->getAvailableSemaphore();]])

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
        // PlagC: acquire on demand after gameplay had time to overlap presentation.
        if( mSwapchainStatus == SwapchainReleased )
            acquireNextSwapchain();
        OGRE_ASSERT_LOW( mSwapchainStatus != SwapchainReleased );]])

    replace([[        if( mSwapchainStatus == SwapchainAcquired || mSwapchainStatus == SwapchainPendingSwap )
        {
            // Ogre never rendered to this window. There's nothing to present.]], [[        if( mSwapchainStatus == SwapchainReleased || mSwapchainStatus == SwapchainAcquired ||
            mSwapchainStatus == SwapchainPendingSwap )
        {
            // Released is a valid idle state with lazy acquisition; there is nothing to present.]])

    replace('    void VulkanWindowSwapChainBased::_swapBuffers( VkSemaphore queueFinishSemaphore )',
            '    uint32 VulkanWindowSwapChainBased::_swapBuffers( VkSemaphore queueFinishSemaphore )')

    replace([[        VkPresentInfoKHR present;
        makeVkStruct( present, VK_STRUCTURE_TYPE_PRESENT_INFO_KHR );
        present.swapchainCount = 1u;
        present.pSwapchains = &mSwapchain;
        present.pImageIndices = &currentSwapchainIdx;
        if( queueFinishSemaphore )
        {
            present.waitSemaphoreCount = 1u;
            present.pWaitSemaphores = &queueFinishSemaphore;
        }

        VkResult result = vkQueuePresentKHR( mDevice->mPresentQueue, &present );

        if( result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR )
        {
            LogManager::getSingleton().logMessage(
                "[VulkanWindowSwapChainBased::swapBuffers] vkQueuePresentKHR: error presenting VkResult "
                "= " +
                vkResultToString( result ) );
        }

        if( result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR )
            mSuboptimal = true;

        mSwapchainStatus = SwapchainReleased;]], [[        // vkQueuePresentKHR may block in Windows/AMD WSI. Run that host wait on the worker.
        const auto plagcEnqueueStart = std::chrono::steady_clock::now();
        const bool serializeWithGraphicsQueue =
            mDevice->mPresentQueue == mDevice->mGraphicsQueue.mQueue;
        PlagCVulkanPresentScheduler::get().enqueue(
            mDevice->mPresentQueue, mSwapchain, queueFinishSemaphore, currentSwapchainIdx,
            serializeWithGraphicsQueue );
        const double plagcEnqueueMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - plagcEnqueueStart ).count();
        static double plagcEnqueueTotalMs = 0.0;
        static double plagcEnqueueMaxMs = 0.0;
        static uint32 plagcEnqueueSamples = 0u;
        plagcEnqueueTotalMs += plagcEnqueueMs;
        plagcEnqueueMaxMs = std::max( plagcEnqueueMaxMs, plagcEnqueueMs );
        if( ++plagcEnqueueSamples == 240u )
        {
            LogManager::getSingleton().logMessage(
                "[PlagC Vulkan CPU] present-enqueue avg=" +
                StringConverter::toString( plagcEnqueueTotalMs / double( plagcEnqueueSamples ) ) +
                " ms max=" + StringConverter::toString( plagcEnqueueMaxMs ) + " ms" );
            plagcEnqueueSamples = 0u;
            plagcEnqueueTotalMs = 0.0;
            plagcEnqueueMaxMs = 0.0;
        }

        mSwapchainStatus = SwapchainReleased;]])

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
