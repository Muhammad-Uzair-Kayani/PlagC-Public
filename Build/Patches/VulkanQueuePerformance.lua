-- Pipelined WSI patch for the pinned Ogre-Next Vulkan queue.
-- Ogre 3.0 presents and immediately blocks acquiring the next image inside the same
-- renderOneFrame. The paired VulkanWindowPerformance patch acquires lazily when the next
-- frame actually needs the swapchain image, allowing game/physics/animation CPU work to
-- overlap presentation-engine progress.
return function(source)
    source = source:gsub("\r\n", "\n")
    local function replace(old, new)
        local first, last = source:find(old, 1, true)
        assert(first, 'Pinned Ogre VulkanQueue changed: ' .. old)
        assert(not source:find(old, last + 1, true), 'Ambiguous VulkanQueue performance patch: ' .. old)
        source = source:sub(1, first - 1) .. new .. source:sub(last + 1)
    end

    replace([[        if( submissionType >= SubmissionType::EndFrameAndSwap )
        {
            // acquireNextSwapchain must be called after newCommandBuffer()
            for( size_t windowIdx = 0u; windowIdx < numWindowsPendingSwap; ++windowIdx )
                mWindowsPendingSwap[windowIdx]->acquireNextSwapchain();
            mWindowsPendingSwap.clear();

            mGpuSignalSemaphForCurrCmdBuff.clear();
        }]], [[        if( submissionType >= SubmissionType::EndFrameAndSwap )
        {
            // PlagC: do not acquire the next image immediately after presenting. The window
            // acquires on first use next frame, after gameplay had time to run in parallel
            // with WSI/display progress. This removes the present -> blocking acquire bubble
            // from the tail of renderOneFrame without changing GPU semaphore ordering.
            mWindowsPendingSwap.clear();
            mGpuSignalSemaphForCurrCmdBuff.clear();
        }]])

    return source
end
