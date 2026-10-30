-- Common renderer source generation for non-Vulkan builds.
-- Vulkan already owns a generated OgreRenderer path in Build/Vulkan.lua; this wrapper
-- preserves that path and gives OpenGL the same workspace-local editor UI hook.
local vulkanEngineSourcesImpl = vulkanEngineSources
local root = path.getabsolute('..')
local generated = root .. '/Bin-Int/Generated'

local function read(file)
    local f = assert(io.open(file, 'rb'))
    local data = f:read('*a')
    f:close()
    return data:gsub('\r\n', '\n')
end

local function writeIfChanged(file, data)
    os.mkdir(path.getdirectory(file))
    local existing = io.open(file, 'rb')
    local previous = existing and existing:read('*a') or nil
    if existing then existing:close() end
    if previous == data then return end
    local out = assert(io.open(file, 'wb'))
    out:write(data)
    out:close()
end

function vulkanEngineSources()
    if PLAGC_VULKAN then
        return vulkanEngineSourcesImpl()
    end

    local original = root .. '/PlagC/src/Renderer/OgreRenderer.cpp'
    local source = read(original)
    local old = [[    Detail::SetOgreUIOutput(this, p.output ? p.output->getTexture() : nullptr);
    if (!p.root->renderOneFrame())]]
    local new = [[    Detail::SetOgreUIOutput(this, p.output ? p.output->getTexture() : nullptr);
    Detail::OgreUIWorkspaceScope uiWorkspace(this, p.workspace);
    if (!p.root->renderOneFrame())]]

    local first, last = source:find(old, 1, true)
    assert(first, 'Common Ogre renderer UI hook changed')
    assert(not source:find(old, last + 1, true), 'Ambiguous common Ogre renderer UI hook')
    source = source:sub(1, first - 1) .. new .. source:sub(last + 1)

    local output = generated .. '/PlagCOgreRenderer.cpp'
    writeIfChanged(output, source)
    removefiles { original }
    files { output }
    -- The generated translation unit retains private renderer includes such as
    -- UiSubmissionInternal.h from the original source directory.
    includedirs { root .. '/PlagC/src/Renderer' }
end
