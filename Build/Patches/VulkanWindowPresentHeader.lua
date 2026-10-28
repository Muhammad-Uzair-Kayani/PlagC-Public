-- Backport Ogre's swapchain-index return used for safe presentation semaphore recycling.
return function(source)
    source = source:gsub("\r\n", "\n")
    local old = '        void _swapBuffers( VkSemaphore queueFinishSemaphore );'
    local new = '        uint32 _swapBuffers( VkSemaphore queueFinishSemaphore );'
    local first, last = source:find(old, 1, true)
    assert(first, 'Pinned Ogre VulkanWindow header changed')
    assert(not source:find(old, last + 1, true), 'Ambiguous VulkanWindow header patch')
    return source:sub(1, first - 1) .. new .. source:sub(last + 1)
end
