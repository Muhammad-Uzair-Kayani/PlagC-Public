-- Vulkan queue throughput patch for the pinned Ogre-Next 3.0 backend.
-- We reserve a second queue from the *same graphics family* when one exists. Because
-- presentation support is a queue-family property, this is safe anywhere Ogre's existing
-- assumption that the graphics family can present is already valid. Hardware exposing only
-- one graphics queue falls back byte-for-byte to Ogre's original single-queue behaviour.
return function(source)
    source = source:gsub("\r\n", "\n")
    local function replace(old, new)
        local first, last = source:find(old, 1, true)
        assert(first, 'Pinned Ogre VulkanDevice changed: ' .. old)
        assert(not source:find(old, last + 1, true), 'Ambiguous VulkanDevice queue patch: ' .. old)
        source = source:sub(1, first - 1) .. new .. source:sub(last + 1)
    end

    replace([[        findGraphicsQueue( usedQueueCount );
        findComputeQueue( usedQueueCount, maxComputeQueues );]], [[        findGraphicsQueue( usedQueueCount );

        // PlagC: reserve another queue from the graphics family for WSI when the family
        // exposes one. vkQueuePresentKHR then no longer shares the exact VkQueue object used
        // by vkQueueSubmit. We deliberately keep the same family so no queue-family ownership
        // transfer is required for swapchain images.
        static uint32 sPlagCPresentFamily = 0xFFFFFFFFu;
        static uint32 sPlagCPresentIndex = 0xFFFFFFFFu;
        sPlagCPresentFamily = 0xFFFFFFFFu;
        sPlagCPresentIndex = 0xFFFFFFFFu;
        const uint32 graphicsFamily = mGraphicsQueue.mFamilyIdx;
        if( graphicsFamily < usedQueueCount.size() &&
            usedQueueCount[graphicsFamily] < mQueueProps[graphicsFamily].queueCount )
        {
            sPlagCPresentFamily = graphicsFamily;
            sPlagCPresentIndex = usedQueueCount[graphicsFamily]++;
        }

        findComputeQueue( usedQueueCount, maxComputeQueues );]])

    -- The function-local statics above cannot be referenced from initQueues, therefore persist
    -- the selected queue indices in translation-unit helpers. Replace them with TU statics while
    -- preserving the exact selection logic. This two-step form keeps the anchor close to upstream.
    replace([[namespace Ogre
{
    static FastArray<IdString> msInstanceExtensions;]], [[namespace Ogre
{
    static FastArray<IdString> msInstanceExtensions;
    static uint32 gPlagCPresentQueueFamily = 0xFFFFFFFFu;
    static uint32 gPlagCPresentQueueIndex = 0xFFFFFFFFu;]])

    replace([[        static uint32 sPlagCPresentFamily = 0xFFFFFFFFu;
        static uint32 sPlagCPresentIndex = 0xFFFFFFFFu;
        sPlagCPresentFamily = 0xFFFFFFFFu;
        sPlagCPresentIndex = 0xFFFFFFFFu;]], [[        gPlagCPresentQueueFamily = 0xFFFFFFFFu;
        gPlagCPresentQueueIndex = 0xFFFFFFFFu;]])
    replace('            sPlagCPresentFamily = graphicsFamily;\n            sPlagCPresentIndex = usedQueueCount[graphicsFamily]++;',
            '            gPlagCPresentQueueFamily = graphicsFamily;\n            gPlagCPresentQueueIndex = usedQueueCount[graphicsFamily]++;')

    replace([[            TODO_findRealPresentQueue;
            mPresentQueue = mGraphicsQueue.mQueue;]], [[            if( gPlagCPresentQueueFamily != 0xFFFFFFFFu )
            {
                vkGetDeviceQueue( mDevice, gPlagCPresentQueueFamily, gPlagCPresentQueueIndex,
                                  &mPresentQueue );
                LogManager::getSingleton().logMessage(
                    "[PlagC Vulkan] Dedicated present queue enabled: family=" +
                    StringConverter::toString( gPlagCPresentQueueFamily ) + " index=" +
                    StringConverter::toString( gPlagCPresentQueueIndex ) );
            }
            else
            {
                // One-queue GPUs retain Ogre's original, fully supported path.
                mPresentQueue = mGraphicsQueue.mQueue;
                LogManager::getSingleton().logMessage(
                    "[PlagC Vulkan] Dedicated present queue unavailable; using graphics queue" );
            }]])

    return source
end
