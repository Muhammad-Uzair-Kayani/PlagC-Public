-- Reserve an unused second queue from the same graphics family for presentation when possible.
-- Same-family queues require no swapchain image ownership transfer. If async compute already
-- consumed the extra queue, or the family exposes only one queue, Ogre's original graphics
-- queue remains the present queue.
return function(source)
    source = source:gsub("\r\n", "\n")
    local function replace(old, new)
        local first, last = source:find(old, 1, true)
        assert(first, 'Pinned Ogre VulkanDevice changed: ' .. old)
        assert(not source:find(old, last + 1, true), 'Ambiguous VulkanDevice queue patch: ' .. old)
        source = source:sub(1, first - 1) .. new .. source:sub(last + 1)
    end

    replace([[namespace Ogre
{
    static FastArray<IdString> msInstanceExtensions;]], [[namespace Ogre
{
    static FastArray<IdString> msInstanceExtensions;
    static uint32 gPlagCPresentQueueFamily = 0xFFFFFFFFu;
    static uint32 gPlagCPresentQueueIndex = 0xFFFFFFFFu;]])

    replace([[        findGraphicsQueue( usedQueueCount );
        findComputeQueue( usedQueueCount, maxComputeQueues );
        findTransferQueue( usedQueueCount, maxTransferQueues );]], [[        findGraphicsQueue( usedQueueCount );
        findComputeQueue( usedQueueCount, maxComputeQueues );
        findTransferQueue( usedQueueCount, maxTransferQueues );

        // PlagC: use a spare queue from the graphics family for presentation. Reserve it only
        // after Ogre's requested async queues, so this optimization never steals compute/transfer
        // capacity from an engine configuration that explicitly asked for it.
        gPlagCPresentQueueFamily = 0xFFFFFFFFu;
        gPlagCPresentQueueIndex = 0xFFFFFFFFu;
        const uint32 graphicsFamily = mGraphicsQueue.mFamilyIdx;
        if( graphicsFamily < usedQueueCount.size() &&
            usedQueueCount[graphicsFamily] < mQueueProps[graphicsFamily].queueCount )
        {
            gPlagCPresentQueueFamily = graphicsFamily;
            gPlagCPresentQueueIndex = usedQueueCount[graphicsFamily]++;
        }]])

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
                mPresentQueue = mGraphicsQueue.mQueue;
                LogManager::getSingleton().logMessage(
                    "[PlagC Vulkan] Dedicated present queue unavailable; using graphics queue" );
            }]])

    return source
end
