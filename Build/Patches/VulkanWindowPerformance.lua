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

    return source
end
