-- Applied to the pinned Ogre source at project generation; Vendor stays immutable.
-- Keep this pure Lua so the regression harness can exercise the exact generated code.
return function(source)
    local function replace(old, new)
        local first, last = source:find(old, 1, true)
        assert(first, 'Pinned Ogre Vulkan allocator changed: ' .. old)
        assert(not source:find(old, last + 1, true), 'Ambiguous Vulkan allocator patch: ' .. old)
        source = source:sub(1, first - 1) .. new .. source:sub(last + 1)
    end
    replace('#include "../OgreVulkanDelayedFuncs.h"', '#include "OgreVulkanDelayedFuncs.h"')
    -- DEVICE_LOCAL is a preference for GPU-only buffers, not a Vulkan requirement.
    -- Append shared heaps AFTER local heaps, preserving the existing fast path.
    replace('        mSupportsNonCoherentMemory = !mBestVkMemoryTypeIndex[CPU_WRITE_PERSISTENT].empty();', [[        // PlagC: retain compatible shared heaps when the local heap fills on UMA GPUs.
        for( uint32 i = 0u; i < numMemoryTypes; ++i )
        {
            if( ( supportedMemoryTypesBuffer & ( 1u << i ) ) &&
                !( memProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) )
                addMemoryType( CPU_INACCESSIBLE, memProperties, i );
        }

        mSupportsNonCoherentMemory = !mBestVkMemoryTypeIndex[CPU_WRITE_PERSISTENT].empty();]])
    -- Subtraction avoids unsigned overflow and accepts an exact fit. In the texture
    -- fallback poolSize is still SIZE_MAX here: testing it can falsely reject a heap.
    replace('if( mUsedHeapMemory[heapIdx] + sizeBytes < memHeaps[heapIdx].size )', [[if( mUsedHeapMemory[heapIdx] <= memHeaps[heapIdx].size &&
                            sizeBytes <= memHeaps[heapIdx].size - mUsedHeapMemory[heapIdx] )]])
    replace('if( mUsedHeapMemory[heapIdx] + poolSize < memHeaps[heapIdx].size )', [[if( mUsedHeapMemory[heapIdx] <= memHeaps[heapIdx].size &&
                                    sizeBytes <= memHeaps[heapIdx].size - mUsedHeapMemory[heapIdx] )]])
    replace([[                        OGRE_EXCEPT( Exception::ERR_RENDERINGAPI_ERROR,
                                     "Out of Memory trying to allocate " +]], [[                        for( uint32 heap = 0u;
                             heap < mDevice->mDeviceMemoryProperties.memoryHeapCount; ++heap )
                        {
                            LogManager::getSingleton().logMessage(
                                "[PlagC Vulkan] allocation rejected: heap=" +
                                StringConverter::toString( heap ) + " reserved=" +
                                StringConverter::toString( mUsedHeapMemory[heap] ) + " capacity=" +
                                StringConverter::toString( memHeaps[heap].size ), LML_CRITICAL );
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
                LogManager::getSingleton().logMessage(
                    "[PlagC Vulkan] GPU pool: type=" +
                    StringConverter::toString( chosenMemoryTypeIdx ) + " heap=" +
                    StringConverter::toString( memTypes[chosenMemoryTypeIdx].heapIndex ) +
                    " bytes=" + StringConverter::toString( poolSize ) + " deviceLocal=" +
                    StringConverter::toString( ( memTypes[chosenMemoryTypeIdx].propertyFlags &
                                                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT ) != 0u ) );
            }
            const size_t usablePoolSize = poolSize;]])
    return source
end
