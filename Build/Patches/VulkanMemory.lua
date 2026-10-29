-- Applied to the pinned Ogre source at project generation; Vendor stays immutable.
-- Keep this pure Lua so the regression harness can exercise the exact generated code.
return function(source)
    -- Binary reads preserve Windows checkout CRLFs; Lua long strings use LF.
    source = source:gsub("\r\n", "\n")
    local function replace(old, new)
        local first, last = source:find(old, 1, true)
        assert(first, 'Pinned Ogre Vulkan allocator changed: ' .. old)
        assert(not source:find(old, last + 1, true), 'Ambiguous Vulkan allocator patch: ' .. old)
        source = source:sub(1, first - 1) .. new .. source:sub(last + 1)
    end
    replace('#include "../OgreVulkanDelayedFuncs.h"', '#include "OgreVulkanDelayedFuncs.h"')

    -- DEVICE_LOCAL remains the first-choice location. Shared memory is retained only as a
    -- compatible last-resort on UMA GPUs where the small local aperture cannot hold the scene.
    replace('        mSupportsNonCoherentMemory = !mBestVkMemoryTypeIndex[CPU_WRITE_PERSISTENT].empty();', [[        // PlagC: retain HOST_VISIBLE shared heaps after all device-local candidates.
        for( uint32 i = 0u; i < numMemoryTypes; ++i )
        {
            const VkMemoryPropertyFlags flags = memProperties.memoryTypes[i].propertyFlags;
            if( ( supportedMemoryTypesBuffer & ( 1u << i ) ) &&
                !( flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) &&
                ( flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT ) )
                addMemoryType( CPU_INACCESSIBLE, memProperties, i );
        }

        mSupportsNonCoherentMemory = !mBestVkMemoryTypeIndex[CPU_WRITE_PERSISTENT].empty();]])

    -- Build a conservative per-heap availability table. When VK_EXT_memory_budget is enabled,
    -- combine Ogre's own reservation accounting with the driver's current budget/usage estimate.
    replace([[            const VkMemoryHeap *memHeaps = mDevice->mDeviceMemoryProperties.memoryHeaps;
            const VkMemoryType *memTypes = mDevice->mDeviceMemoryProperties.memoryTypes;]], [[            const VkMemoryHeap *memHeaps = mDevice->mDeviceMemoryProperties.memoryHeaps;
            const VkMemoryType *memTypes = mDevice->mDeviceMemoryProperties.memoryTypes;
            size_t availableHeapMemory[16] = {};
            for( uint32 heap = 0u; heap < mDevice->mDeviceMemoryProperties.memoryHeapCount; ++heap )
            {
                availableHeapMemory[heap] =
                    mUsedHeapMemory[heap] <= memHeaps[heap].size
                        ? static_cast<size_t>( memHeaps[heap].size - mUsedHeapMemory[heap] )
                        : 0u;
            }
#if defined(VK_EXT_memory_budget)
            if( mDevice->hasDeviceExtension( VK_EXT_MEMORY_BUDGET_EXTENSION_NAME ) &&
                VulkanDevice::hasInstanceExtension(
                    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME ) )
            {
                VkPhysicalDeviceMemoryBudgetPropertiesEXT budgetProperties;
                makeVkStruct( budgetProperties,
                              VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT );
                VkPhysicalDeviceMemoryProperties2 memoryProperties2;
                makeVkStruct( memoryProperties2, VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2 );
                memoryProperties2.pNext = &budgetProperties;
                PFN_vkGetPhysicalDeviceMemoryProperties2KHR getMemoryProperties2 =
                    reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties2KHR>(
                        vkGetInstanceProcAddr( mDevice->mInstance,
                                               "vkGetPhysicalDeviceMemoryProperties2KHR" ) );
                if( getMemoryProperties2 )
                {
                    getMemoryProperties2( mDevice->mPhysicalDevice, &memoryProperties2 );
                    for( uint32 heap = 0u;
                         heap < mDevice->mDeviceMemoryProperties.memoryHeapCount; ++heap )
                    {
                        const VkDeviceSize budget = budgetProperties.heapBudget[heap];
                        const VkDeviceSize usage = budgetProperties.heapUsage[heap];
                        const size_t driverAvailable =
                            budget > usage ? static_cast<size_t>( budget - usage ) : 0u;
                        availableHeapMemory[heap] =
                            std::min( availableHeapMemory[heap], driverAvailable );
                    }
                }
            }
#endif]])

    -- Exact-fit safe checks; use the current budget rather than nominal heap capacity.
    replace('if( mUsedHeapMemory[heapIdx] + sizeBytes < memHeaps[heapIdx].size )',
            'if( sizeBytes <= availableHeapMemory[heapIdx] )')
    replace('if( mUsedHeapMemory[heapIdx] + poolSize < memHeaps[heapIdx].size )',
            'if( sizeBytes <= availableHeapMemory[heapIdx] )')
    replace([[                                std::min( (VkDeviceSize)mDefaultPoolSize[vboFlag],
                                          memHeaps[heapIdx].size - mUsedHeapMemory[heapIdx] );]], [[                                std::min( static_cast<size_t>( mDefaultPoolSize[vboFlag] ),
                                          availableHeapMemory[heapIdx] );
                            // A fallback into shared system RAM must not reserve another full 64 MiB
                            // for a small resource. This was the source of the repeated 64 MiB pools
                            // visible on the Radeon iGPU log.
                            if( !( memTypes[*itMemTypeIdx].propertyFlags &
                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) &&
                                ( vboFlag == CPU_INACCESSIBLE || vboFlag == TEXTURES_OPTIMAL ) )
                                defaultPoolSize = std::min<size_t>(
                                    defaultPoolSize, std::max<size_t>( sizeBytes, 8u * 1024u * 1024u ) );]])
    replace([[                                    std::min( (VkDeviceSize)mDefaultPoolSize[vboFlag],
                                                  memHeaps[heapIdx].size - mUsedHeapMemory[heapIdx] );]], [[                                    std::min( static_cast<size_t>( mDefaultPoolSize[vboFlag] ),
                                              availableHeapMemory[heapIdx] );
                                    if( !( memTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) &&
                                        ( vboFlag == CPU_INACCESSIBLE || vboFlag == TEXTURES_OPTIMAL ) )
                                    {
                                        defaultPoolSize = std::min<size_t>(
                                            defaultPoolSize,
                                            std::max<size_t>( sizeBytes, 8u * 1024u * 1024u ) );
                                    }]])

    replace([[                        OGRE_EXCEPT( Exception::ERR_RENDERINGAPI_ERROR,
                                     "Out of Memory trying to allocate " +]], [[                        for( uint32 heap = 0u;
                             heap < mDevice->mDeviceMemoryProperties.memoryHeapCount; ++heap )
                        {
                            LogManager::getSingleton().logMessage(
                                "[PlagC Vulkan] allocation rejected: heap=" +
                                StringConverter::toString( heap ) + " reserved=" +
                                StringConverter::toString( mUsedHeapMemory[heap] ) + " capacity=" +
                                StringConverter::toString( memHeaps[heap].size ) + " available=" +
                                StringConverter::toString( availableHeapMemory[heap] ), LML_CRITICAL );
                        }
                        LogManager::getSingleton().logMessage(
                            "[PlagC Vulkan] requested=" + StringConverter::toString( sizeBytes ) +
                            " class=" + StringConverter::toString( static_cast<uint32>( vboFlag ) ) +
                            " allowedTypes=" + StringConverter::toString( textureMemTypeBits ) +
                            " preferredTypes=" + StringConverter::toString( mMemoryTypesInUse[vboFlag] ),
                            LML_CRITICAL );
                        OGRE_EXCEPT( Exception::ERR_RENDERINGAPI_ERROR,
                                     "Out of Memory trying to allocate " +]])

    replace('            const size_t usablePoolSize = poolSize;', [[            if( vboFlag == CPU_INACCESSIBLE || vboFlag == TEXTURES_OPTIMAL )
            {
                // Texture-only memory requirements can force a dedicated VkDeviceMemory block.
                // Label those separately so diagnostics do not mistake each texture allocation
                // for a reusable 8/16/24 MiB buffer-pool leak.
                const String allocationKind = bIsTextureOnly ? "dedicated-texture" : "buffer-pool";
                LogManager::getSingleton().logMessage(
                    "[PlagC Vulkan] GPU allocation: kind=" + allocationKind + " class=" +
                    StringConverter::toString( static_cast<uint32>( vboFlag ) ) + " type=" +
                    StringConverter::toString( chosenMemoryTypeIdx ) + " heap=" +
                    StringConverter::toString( memTypes[chosenMemoryTypeIdx].heapIndex ) +
                    " requested=" + StringConverter::toString( sizeBytes ) + " reserved=" +
                    StringConverter::toString( poolSize ) + " deviceLocal=" +
                    StringConverter::toString( ( memTypes[chosenMemoryTypeIdx].propertyFlags &
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) != 0u ) );
            }
            const size_t usablePoolSize = poolSize;]])

    -- Keep delayed-free telemetry cheap and periodic. Do not lower Ogre's 512 MiB barrier
    -- threshold blindly: first prove whether the backlog grows during gameplay or is mostly
    -- teardown. The peak survives flushes so repeated load/unload tests remain diagnostic.
    replace('        deallocateEmptyVbos( false );', [[        static size_t plagcDelayedPeakBytes = 0u;
        static uint32 plagcDelayedTelemetryFrames = 0u;
        plagcDelayedPeakBytes = std::max( plagcDelayedPeakBytes, mDelayedBlocksSize );
        if( ++plagcDelayedTelemetryFrames >= 240u )
        {
            if( mDelayedBlocksSize || plagcDelayedPeakBytes )
            {
                LogManager::getSingleton().logMessage(
                    "[PlagC Vulkan Memory] delayed=" +
                    StringConverter::toString( mDelayedBlocksSize ) + " peak=" +
                    StringConverter::toString( plagcDelayedPeakBytes ) + " threshold=" +
                    StringConverter::toString( mDelayedBlocksFlushThreshold ) );
            }
            plagcDelayedTelemetryFrames = 0u;
        }

        deallocateEmptyVbos( false );]])
    return source
end
