-- Backport Ogre's 2025 presentation-semaphore lifetime fix to the pinned 3.0 Vulkan VaoManager.
-- VkPresentInfoKHR wait semaphores cannot safely be recycled based only on graphics frame age;
-- the presentation engine releases them when the corresponding swapchain image is acquired again.
return function(source)
    source = source:gsub("\r\n", "\n")
    local function replace(old, new)
        local first, last = source:find(old, 1, true)
        assert(first, 'Pinned/generated Ogre VulkanVaoManager changed: ' .. old)
        assert(not source:find(old, last + 1, true), 'Ambiguous Vulkan presentation semaphore patch: ' .. old)
        source = source:sub(1, first - 1) .. new .. source:sub(last + 1)
    end

    replace([[        {
            FastArray<UsedSemaphore>::const_iterator itor = mUsedSemaphores.begin();
            FastArray<UsedSemaphore>::const_iterator endt = mUsedSemaphores.end();

            while( itor != endt )
            {
                vkDestroySemaphore( mDevice->mDevice, itor->semaphore, 0 );
                ++itor;
            }
        }

        deleteStagingBuffers();]], [[        {
            FastArray<UsedSemaphore>::const_iterator itor = mUsedSemaphores.begin();
            FastArray<UsedSemaphore>::const_iterator endt = mUsedSemaphores.end();

            while( itor != endt )
            {
                vkDestroySemaphore( mDevice->mDevice, itor->semaphore, 0 );
                ++itor;
            }
        }
        {
            FastArray<UsedSemaphore>::const_iterator itor = mUsedPresentSemaphores.begin();
            FastArray<UsedSemaphore>::const_iterator endt = mUsedPresentSemaphores.end();
            while( itor != endt )
            {
                vkDestroySemaphore( mDevice->mDevice, itor->semaphore, 0 );
                ++itor;
            }
        }

        deleteStagingBuffers();]])

    replace([[        while( itor != endt )
            mUsedSemaphores.push_back( UsedSemaphore( *itor++, mFrameCount ) );
    }
    //-----------------------------------------------------------------------------------
    void VulkanVaoManager::notifySemaphoreUnused( VkSemaphore semaphore )]], [[        while( itor != endt )
            mUsedSemaphores.push_back( UsedSemaphore( *itor++, mFrameCount ) );
    }
    //-----------------------------------------------------------------------------------
    void VulkanVaoManager::notifyPresentationWaitSemaphoreSubmitted( VkSemaphore semaphore,
                                                                     uint32 swapchainIdx )
    {
        mUsedPresentSemaphores.push_back( UsedSemaphore( semaphore, swapchainIdx ) );
    }
    //-----------------------------------------------------------------------------------
    void VulkanVaoManager::notifySwapchainIndexAcquired( uint32 swapchainIdx )
    {
        FastArray<UsedSemaphore>::iterator itor = mUsedPresentSemaphores.begin();
        FastArray<UsedSemaphore>::iterator endt = mUsedPresentSemaphores.end();
        while( itor != endt )
        {
            if( itor->frame == swapchainIdx )
            {
                mAvailableSemaphores.push_back( itor->semaphore );
                itor = efficientVectorRemove( mUsedPresentSemaphores, itor );
                endt = mUsedPresentSemaphores.end();
            }
            else
                ++itor;
        }
    }
    //-----------------------------------------------------------------------------------
    void VulkanVaoManager::notifySemaphoreUnused( VkSemaphore semaphore )]])

    return source
end
