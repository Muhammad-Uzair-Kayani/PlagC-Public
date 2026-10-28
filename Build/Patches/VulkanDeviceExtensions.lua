-- Enable memory-budget reporting on the pinned Ogre Vulkan render system when the driver exposes it.
-- The pinned 3.0 source selects optional device extensions in OgreVulkanRenderSystem.cpp.
return function(source)
    source = source:gsub("\r\n", "\n")
    local old = [[                    else if( extensionName == VK_KHR_16BIT_STORAGE_EXTENSION_NAME )
                        deviceExtensions.push_back( VK_KHR_16BIT_STORAGE_EXTENSION_NAME );
                    else if( extensionName == VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME )
                        deviceExtensions.push_back( VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME );]]
    local new = [[                    else if( extensionName == VK_KHR_16BIT_STORAGE_EXTENSION_NAME )
                        deviceExtensions.push_back( VK_KHR_16BIT_STORAGE_EXTENSION_NAME );
                    else if( extensionName == VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME )
                        deviceExtensions.push_back( VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME );
                    else if( extensionName == VK_EXT_MEMORY_BUDGET_EXTENSION_NAME )
                        deviceExtensions.push_back( VK_EXT_MEMORY_BUDGET_EXTENSION_NAME );]]
    local first, last = source:find(old, 1, true)
    assert(first, 'Pinned Ogre Vulkan render-system extension selection changed')
    assert(not source:find(old, last + 1, true), 'Ambiguous Vulkan render-system extension selection')
    return source:sub(1, first - 1) .. new .. source:sub(last + 1)
end
