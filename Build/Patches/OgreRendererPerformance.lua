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
#include <PlagC/GpuProfiler.h>
#include <cstdlib>]])

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
        // Four frame slots is the measured stable default on the Radeon iGPU, but keep the
        // value tunable so 2/3/4 frame-in-flight WSI experiments do not require recompiling.
        unsigned long framesInFlight = 4ul;
        if (const char *framesEnv = std::getenv("PC_VULKAN_FRAMES_IN_FLIGHT"))
        {
            char *parseEnd = nullptr;
            const unsigned long parsed = std::strtoul(framesEnv, &parseEnd, 10);
            if (parseEnd != framesEnv && *parseEnd == '\0' && parsed >= 2ul && parsed <= 4ul)
                framesInFlight = parsed;
        }
        options["VaoManager::mDynamicBufferMultiplier"] = std::to_string(framesInFlight);
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

    replace('        std::vector<std::uint64_t> users{};', [[        std::vector<std::uint64_t> users{};
        std::vector<Vertex> pendingVertices{};
        bool uploadQueued = false;]])
    replace('    std::string workspaceName;', [[    std::string workspaceName;
    std::vector<std::uint64_t> pendingMeshUploads;]])

    -- Validate and compute bounds in one traversal, before mutating any mesh state.
    replace([[    for (auto &v : vertices)
        Require(Finite(v.position) && Finite(v.normal) && std::isfinite(v.u) && std::isfinite(v.v),
                "nonfinite mesh update");]], [[    Ogre::Vector3 lo = V(vertices[0].position), hi = lo;
    for (const auto &v : vertices)
    {
        Require(Finite(v.position) && Finite(v.normal) && std::isfinite(v.u) && std::isfinite(v.v),
                "nonfinite mesh update");
        lo.makeFloor(V(v.position));
        hi.makeCeil(V(v.position));
    }]])
    replace([[    Ogre::Vector3 lo = V(vertices[0].position), hi = lo;
    for (auto &v : vertices)
    {
        lo.makeFloor(V(v.position));
        hi.makeCeil(V(v.position));
    }
]], '')

    replace('    buffer->upload(vertices.data(), 0, vertices.size());', [[    if (backend_ == RendererBackend::Vulkan &&
        buffer->getBufferType() >= Ogre::BT_DYNAMIC_PERSISTENT)
    {
        // Own the data until submission. Updating twice must not rotate the GPU ring twice.
        mesh.pendingVertices.assign(vertices.begin(), vertices.end());
        if (!mesh.uploadQueued)
        {
            impl_->pendingMeshUploads.push_back(handle.value);
            mesh.uploadQueued = true;
        }
    }
    else
        buffer->upload(vertices.data(), 0, vertices.size());]])

    replace('    if (!p.root->renderOneFrame())', [[    // Visit only dirty mesh IDs. IDs are never reused; a mesh destroyed before rendering
    // leaves a harmless skipped entry. Keep CPU vector capacity for the next animated pose.
    for (auto id : p.pendingMeshUploads)
    {
        auto found = p.meshes.find(id);
        if (found == p.meshes.end() || found->second.pendingVertices.empty())
            continue;
        auto &mesh = found->second;
        auto *buffer = mesh.value->getSubMesh(0)->mVao[Ogre::VpNormal][0]->getVertexBuffers()[0];
        void *destination = buffer->map(0, mesh.pendingVertices.size());
        std::memcpy(destination, mesh.pendingVertices.data(),
                    mesh.pendingVertices.size() * sizeof(Vertex));
        buffer->unmap(Ogre::UO_KEEP_PERSISTENT);
        mesh.pendingVertices.clear();
        mesh.uploadQueued = false;
    }
    p.pendingMeshUploads.clear();
    Detail::OgreUIWorkspaceScope uiWorkspace(this, p.workspace);
    if (!p.root->renderOneFrame())]])

    -- UO_KEEP_PERSISTENT intentionally leaves Ogre's MappingState persistent even though no
    -- map range is active. Ogre's BufferPacked destructor treats that as a bad lifetime and
    -- emits the "Deleting mapped buffer" warning. Final-unmap engine-owned dynamic meshes
    -- before releasing their MeshPtr, matching Ogre's own persistent-buffer destruction code.
    replace([[            objects.clear();
            lights.clear();
            meshes.clear();
            materials.clear();]], [[            objects.clear();
            lights.clear();
            for (auto &[id, mesh] : meshes)
            {
                if (!mesh.dynamic || !mesh.value || mesh.value->getNumSubMeshes() == 0u)
                    continue;
                auto *buffer =
                    mesh.value->getSubMesh(0)->mVao[Ogre::VpNormal][0]->getVertexBuffers()[0];
                if (buffer && buffer->getMappingState() != Ogre::MS_UNMAPPED)
                    buffer->unmap(Ogre::UO_UNMAP_ALL);
            }
            meshes.clear();
            materials.clear();]])

    replace([[    Ogre::MeshManager::getSingleton().remove(m.value->getHandle());
    p.meshes.erase(h.value);]], [[    if (m.dynamic && m.value && m.value->getNumSubMeshes() != 0u)
    {
        auto *buffer = m.value->getSubMesh(0)->mVao[Ogre::VpNormal][0]->getVertexBuffers()[0];
        if (buffer && buffer->getMappingState() != Ogre::MS_UNMAPPED)
            buffer->unmap(Ogre::UO_UNMAP_ALL);
    }
    Ogre::MeshManager::getSingleton().remove(m.value->getHandle());
    p.meshes.erase(h.value);]])

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
