-- Applied to PlagC's OgreRenderer only in --vulkan builds.
-- Exact replacements fail closed when the engine source changes.
return function(source)
    source = source:gsub("\r\n", "\n")
    local function replace(old, new)
        local first, last = source:find(old, 1, true)
        assert(first, 'PlagC renderer performance patch changed: ' .. old)
        assert(not source:find(old, last + 1, true), 'Ambiguous renderer performance patch: ' .. old)
        source = source:sub(1, first - 1) .. new .. source:sub(last + 1)
    end

    replace('#include <PlagC/Renderer/Renderer.h>', [[#include <PlagC/Renderer/Renderer.h>
#include <PlagC/GpuProfiler.h>]])

    replace('    Ogre::MaterialPtr skyMaterial;\n    RenderStats stats;', [[    Ogre::MaterialPtr skyMaterial;
    // Asset textures are allowed to stream together on Vulkan and are synchronized once
    // before the next rendered frame, instead of forcing a device-facing wait per texture.
    std::size_t pendingTextureUploads = 0;
    RenderStats stats;]])

    replace([[        if (output && !nullBackend)
            workspace = cm->addWorkspace(scene, output->getTexture(), camera, workspaceName, true);
        for (auto &[id, t] : targets)
            t.workspace = cm->addWorkspace(scene, t.value, camera, workspaceName, !nullBackend);]], [[        if (output && !nullBackend)
        {
            workspace = cm->addWorkspace(scene, output->getTexture(), camera, workspaceName, true);
            // The Vulkan profiler records timestamps from compositor pass callbacks.  The
            // listener must be attached to the workspace itself; registering only with the
            // manager does not deliver pass execution callbacks in this Ogre pin.
            GpuProfiler::AttachWorkspace(workspace);
        }
        for (auto &[id, t] : targets)
        {
            t.workspace = cm->addWorkspace(scene, t.value, camera, workspaceName, !nullBackend);
            GpuProfiler::AttachWorkspace(t.workspace);
        }]])

    replace([[    p.output =
        p.root->createRenderWindow("PlagC", window ? unsigned(window->Width()) : 640,
                                   window ? unsigned(window->Height()) : 360, false, &options);]], [[    if (backend_ == RendererBackend::Vulkan)
    {
        // Ogre's persistent dynamic buffers rotate through this many backing regions and
        // waitForTailFrameToFinish() before each map. Three regions were enough on average
        // but produced 16-20 ms tail waits when the Radeon iGPU/present queue briefly fell
        // behind. Four regions also gives the Vulkan swapchain one additional image, keeping
        // vkAcquireNextImageKHR from becoming the same CPU-side bubble.
        options["VaoManager::mDynamicBufferMultiplier"] = "4";
    }
    p.output =
        p.root->createRenderWindow("PlagC", window ? unsigned(window->Width()) : 640,
                                   window ? unsigned(window->Height()) : 360, false, &options);]])

    replace([[    p.output =
        p.root->createRenderWindow("PlagC", window ? unsigned(window->Width()) : 640,
                                   window ? unsigned(window->Height()) : 360, false, &options);
    p.pbs = RegisterHlms<Ogre::HlmsPbs>(*p.root, config.mediaRoot);]], [[    p.output =
        p.root->createRenderWindow("PlagC", window ? unsigned(window->Width()) : 640,
                                   window ? unsigned(window->Height()) : 360, false, &options);
    if (backend_ == RendererBackend::Vulkan)
    {
        // Ogre defaults to 256 MiB of temporary texture staging. OUTPOST ZERO can stage
        // enough decoded textures during bootstrap to cross that threshold, which makes
        // Ogre deliberately stall the GPU. Give the batched bootstrap enough headroom;
        // this is staging memory, not permanent texture residency.
        constexpr std::size_t VulkanStagingBudget = 512ull * 1024ull * 1024ull;
        p.root->getRenderSystem()->getTextureGpuManager()->setStagingTextureMaxBudgetBytes(
            VulkanStagingBudget);
    }
    p.pbs = RegisterHlms<Ogre::HlmsPbs>(*p.root, config.mediaRoot);]])

    replace([[        vb = vm->createVertexBuffer(elements, data.vertices.size(),
                                    dynamic ? Ogre::BT_DEFAULT : Ogre::BT_IMMUTABLE,
                                    const_cast<Vertex *>(data.vertices.data()), false);]], [[        // CPU-skinned meshes are rewritten every frame. BT_DEFAULT is GPU-only in Ogre
        // and therefore turns every Vulkan update into CPU -> staging -> GPU copy traffic.
        // A persistently mapped coherent dynamic buffer uses Ogre's buffered dynamic ring
        // instead and avoids that transfer path on Vulkan/UMA hardware.
        const Ogre::BufferType vertexType =
            dynamic && backend_ == RendererBackend::Vulkan
                ? Ogre::BT_DYNAMIC_PERSISTENT_COHERENT
                : (dynamic ? Ogre::BT_DEFAULT : Ogre::BT_IMMUTABLE);
        vb = vm->createVertexBuffer(elements, data.vertices.size(), vertexType,
                                    const_cast<Vertex *>(data.vertices.data()), false);]])

    replace('    buffer->upload(vertices.data(), 0, vertices.size());', [[    if (backend_ == RendererBackend::Vulkan &&
        buffer->getBufferType() >= Ogre::BT_DYNAMIC_PERSISTENT)
    {
        void *destination = buffer->map(0, vertices.size());
        std::memcpy(destination, vertices.data(), vertices.size_bytes());
        buffer->unmap(Ogre::UO_KEEP_PERSISTENT);
    }
    else
        buffer->upload(vertices.data(), 0, vertices.size());]])

    replace([[        texture->scheduleTransitionTo(Ogre::GpuResidency::Resident, image.release(), true);
        tm->waitForStreamingCompletion();
        impl_->textures.emplace(id, texture);]], [[        texture->scheduleTransitionTo(Ogre::GpuResidency::Resident, image.release(), true);
        impl_->textures.emplace(id, texture);
        if (backend_ == RendererBackend::Vulkan)
            ++impl_->pendingTextureUploads;
        else
            tm->waitForStreamingCompletion();]])

    replace([[    impl_->root->getRenderSystem()->getTextureGpuManager()->destroyTexture(texture);
    impl_->textures.erase(handle.value);]], [[    auto *tm = impl_->root->getRenderSystem()->getTextureGpuManager();
    if (impl_->pendingTextureUploads)
    {
        tm->waitForStreamingCompletion();
        impl_->pendingTextureUploads = 0;
    }
    tm->destroyTexture(texture);
    impl_->textures.erase(handle.value);]])

    replace([[                next->scheduleTransitionTo(Ogre::GpuResidency::Resident, image.release(), true);
                tm->waitForStreamingCompletion();]], [[                next->scheduleTransitionTo(Ogre::GpuResidency::Resident, image.release(), true);
                tm->waitForStreamingCompletion();
                // Streaming completion is global to this texture manager, so it also drains
                // any asset uploads that were queued before the environment replacement.
                p.pendingTextureUploads = 0;]])

    replace([[    if (p.paused)
        return;
    auto start = std::chrono::steady_clock::now();]], [[    if (p.paused)
        return;
    if (p.pendingTextureUploads)
    {
        p.root->getRenderSystem()->getTextureGpuManager()->waitForStreamingCompletion();
        p.pendingTextureUploads = 0;
    }
    auto start = std::chrono::steady_clock::now();]])

    return source
end
