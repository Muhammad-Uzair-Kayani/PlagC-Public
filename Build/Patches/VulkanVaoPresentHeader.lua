-- Backport Ogre's presentation-semaphore lifetime API without modifying the pinned submodule.
return function(source)
    source = source:gsub("\r\n", "\n")
    local function replace(old, new)
        local first, last = source:find(old, 1, true)
        assert(first, 'Pinned Ogre VulkanVaoManager header changed: ' .. old)
        assert(not source:find(old, last + 1, true), 'Ambiguous VulkanVaoManager header patch: ' .. old)
        source = source:sub(1, first - 1) .. new .. source:sub(last + 1)
    end

    replace([[        FastArray<UsedSemaphore> mUsedSemaphores;

        VkSemaphoreArray mAvailableSemaphores;]], [[        FastArray<UsedSemaphore> mUsedSemaphores;
        // Presentation semaphores cannot be recycled merely because N graphics frames passed.
        // The presentation engine releases them when the corresponding swapchain image is
        // acquired again.
        FastArray<UsedSemaphore> mUsedPresentSemaphores;

        VkSemaphoreArray mAvailableSemaphores;]])

    replace([[        void notifyWaitSemaphoreSubmitted( VkSemaphore semaphore );
        void notifyWaitSemaphoresSubmitted( const VkSemaphoreArray &semaphores );
        void notifySemaphoreUnused( VkSemaphore semaphore );]], [[        void notifyWaitSemaphoreSubmitted( VkSemaphore semaphore );
        void notifyWaitSemaphoresSubmitted( const VkSemaphoreArray &semaphores );
        void notifyPresentationWaitSemaphoreSubmitted( VkSemaphore semaphore, uint32 swapchainIdx );
        void notifySwapchainIndexAcquired( uint32 swapchainIdx );
        void notifySemaphoreUnused( VkSemaphore semaphore );]])

    return source
end
