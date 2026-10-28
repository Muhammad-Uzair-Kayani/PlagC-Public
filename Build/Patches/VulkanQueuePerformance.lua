-- Pipelined WSI + CPU submission diagnostics for the pinned Ogre-Next Vulkan queue.
--
-- Presentation is issued by PlagCVulkanPresentScheduler on a worker. If the presentation and
-- graphics handles alias the same VkQueue, Vulkan requires external host synchronization; this
-- patch takes the scheduler's shared mutex around vkQueueSubmit. It also records the queue-side
-- waits that can otherwise be mistaken for command-generation cost inside renderOneFrame.
return function(source)
    source = source:gsub("\r\n", "\n")
    local function replace(old, new)
        local first, last = source:find(old, 1, true)
        assert(first, 'Pinned Ogre VulkanQueue changed: ' .. old)
        assert(not source:find(old, last + 1, true), 'Ambiguous VulkanQueue performance patch: ' .. old)
        source = source:sub(1, first - 1) .. new .. source:sub(last + 1)
    end

    replace('#include <vulkan/vulkan.h>', [[#include <vulkan/vulkan.h>
#include "PlagCVulkanPresentScheduler.h"
#include <algorithm>
#include <chrono>
#include <mutex>]])

    replace([[    void VulkanQueue::newCommandBuffer()
    {
        const size_t currFrame = mVaoManager->waitForTailFrameToFinish();]], [[    void VulkanQueue::newCommandBuffer()
    {
        const auto plagcTailStart = std::chrono::steady_clock::now();
        const size_t currFrame = mVaoManager->waitForTailFrameToFinish();
        const double plagcTailMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - plagcTailStart ).count();
        if( mFamily == Graphics )
        {
            static double plagcTailTotalMs = 0.0;
            static double plagcTailMaxMs = 0.0;
            static uint32 plagcTailSamples = 0u;
            plagcTailTotalMs += plagcTailMs;
            plagcTailMaxMs = std::max( plagcTailMaxMs, plagcTailMs );
            if( ++plagcTailSamples == 240u )
            {
                LogManager::getSingleton().logMessage(
                    "[PlagC Vulkan CPU] new-command-buffer tail-wait avg=" +
                    StringConverter::toString( plagcTailTotalMs / double( plagcTailSamples ) ) +
                    " ms max=" + StringConverter::toString( plagcTailMaxMs ) + " ms" );
                plagcTailSamples = 0u;
                plagcTailTotalMs = 0.0;
                plagcTailMaxMs = 0.0;
            }
        }]])

    replace([[        if( mCurrentCmdBuffer )
        {
            endAllEncoders();

            VkResult result = vkEndCommandBuffer( mCurrentCmdBuffer );
            checkVkResult( result, "vkEndCommandBuffer" );]], [[        if( mCurrentCmdBuffer )
        {
            const auto plagcEndCmdStart = std::chrono::steady_clock::now();
            endAllEncoders();

            VkResult result = vkEndCommandBuffer( mCurrentCmdBuffer );
            checkVkResult( result, "vkEndCommandBuffer" );
            const double plagcEndCmdMs = std::chrono::duration<double, std::milli>(
                std::chrono::steady_clock::now() - plagcEndCmdStart ).count();
            if( mFamily == Graphics )
            {
                static double plagcEndCmdTotalMs = 0.0;
                static double plagcEndCmdMaxMs = 0.0;
                static uint32 plagcEndCmdSamples = 0u;
                plagcEndCmdTotalMs += plagcEndCmdMs;
                plagcEndCmdMaxMs = std::max( plagcEndCmdMaxMs, plagcEndCmdMs );
                if( ++plagcEndCmdSamples == 240u )
                {
                    LogManager::getSingleton().logMessage(
                        "[PlagC Vulkan CPU] end-command-buffer avg=" +
                        StringConverter::toString( plagcEndCmdTotalMs / double( plagcEndCmdSamples ) ) +
                        " ms max=" + StringConverter::toString( plagcEndCmdMaxMs ) + " ms" );
                    plagcEndCmdSamples = 0u;
                    plagcEndCmdTotalMs = 0.0;
                    plagcEndCmdMaxMs = 0.0;
                }
            }]])

    replace([[        const uint8 dynBufferFrame = mVaoManager->waitForTailFrameToFinish();
        VkFence fence = mCurrentFence;  // Note: mCurrentFence may be nullptr

        VkResult result = vkQueueSubmit( mQueue, 1u, &submitInfo, fence );
        checkVkResult( result, "vkQueueSubmit" );]], [[        const auto plagcSubmitTailStart = std::chrono::steady_clock::now();
        const uint8 dynBufferFrame = mVaoManager->waitForTailFrameToFinish();
        const double plagcSubmitTailMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - plagcSubmitTailStart ).count();
        VkFence fence = mCurrentFence;  // Note: mCurrentFence may be nullptr

        // vkQueueSubmit and vkQueuePresentKHR must not execute concurrently when both names refer
        // to the same VkQueue. The worker uses the same mutex around present on that configuration.
        const bool plagcSharesPresentQueue = mOwnerDevice && mQueue == mOwnerDevice->mPresentQueue;
        const auto plagcSubmitStart = std::chrono::steady_clock::now();
        VkResult result;
        {
            std::unique_lock<std::mutex> plagcQueueLock;
            if( plagcSharesPresentQueue )
            {
                plagcQueueLock = std::unique_lock<std::mutex>(
                    PlagCVulkanPresentScheduler::get().sharedGraphicsPresentMutex() );
            }
            result = vkQueueSubmit( mQueue, 1u, &submitInfo, fence );
        }
        const double plagcSubmitMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - plagcSubmitStart ).count();
        checkVkResult( result, "vkQueueSubmit" );

        if( mFamily == Graphics )
        {
            static double plagcSubmitTailTotalMs = 0.0;
            static double plagcSubmitTailMaxMs = 0.0;
            static double plagcSubmitTotalMs = 0.0;
            static double plagcSubmitMaxMs = 0.0;
            static uint32 plagcSubmitSamples = 0u;
            plagcSubmitTailTotalMs += plagcSubmitTailMs;
            plagcSubmitTailMaxMs = std::max( plagcSubmitTailMaxMs, plagcSubmitTailMs );
            plagcSubmitTotalMs += plagcSubmitMs;
            plagcSubmitMaxMs = std::max( plagcSubmitMaxMs, plagcSubmitMs );
            if( ++plagcSubmitSamples == 240u )
            {
                LogManager::getSingleton().logMessage(
                    "[PlagC Vulkan CPU] submit-tail avg=" +
                    StringConverter::toString( plagcSubmitTailTotalMs / double( plagcSubmitSamples ) ) +
                    " ms max=" + StringConverter::toString( plagcSubmitTailMaxMs ) +
                    " ms queue-submit avg=" +
                    StringConverter::toString( plagcSubmitTotalMs / double( plagcSubmitSamples ) ) +
                    " ms max=" + StringConverter::toString( plagcSubmitMaxMs ) + " ms" );
                plagcSubmitSamples = 0u;
                plagcSubmitTailTotalMs = 0.0;
                plagcSubmitTailMaxMs = 0.0;
                plagcSubmitTotalMs = 0.0;
                plagcSubmitMaxMs = 0.0;
            }
        }]])

    replace([[            for( size_t windowIdx = 0u; windowIdx < numWindowsPendingSwap; ++windowIdx )
            {
                VkSemaphore semaphore = mGpuSignalSemaphForCurrCmdBuff[windowsSemaphStart + windowIdx];
                mWindowsPendingSwap[windowIdx]->_swapBuffers( semaphore );
                mVaoManager->notifyWaitSemaphoreSubmitted( semaphore );
            }]], [[            for( size_t windowIdx = 0u; windowIdx < numWindowsPendingSwap; ++windowIdx )
            {
                VkSemaphore semaphore = mGpuSignalSemaphForCurrCmdBuff[windowsSemaphStart + windowIdx];
                const uint32 swapchainIdx = mWindowsPendingSwap[windowIdx]->_swapBuffers( semaphore );
                // Presentation is external to the graphics frame fence. Recycle this semaphore
                // only when vkAcquireNextImageKHR proves that this swapchain image was released.
                mVaoManager->notifyPresentationWaitSemaphoreSubmitted( semaphore, swapchainIdx );
            }]])

    replace([[        if( submissionType >= SubmissionType::EndFrameAndSwap )
        {
            // acquireNextSwapchain must be called after newCommandBuffer()
            for( size_t windowIdx = 0u; windowIdx < numWindowsPendingSwap; ++windowIdx )
                mWindowsPendingSwap[windowIdx]->acquireNextSwapchain();
            mWindowsPendingSwap.clear();

            mGpuSignalSemaphForCurrCmdBuff.clear();
        }]], [[        if( submissionType >= SubmissionType::EndFrameAndSwap )
        {
            // Do not acquire the next image here. Presentation now runs concurrently with
            // gameplay and the window acquires on first render-target use next frame.
            mWindowsPendingSwap.clear();
            mGpuSignalSemaphForCurrCmdBuff.clear();
        }]])

    return source
end
