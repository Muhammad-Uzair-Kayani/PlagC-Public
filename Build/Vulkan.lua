-- Build the pinned shader compiler with the engine CRT, never an SDK compiler binary.
newoption { trigger = 'vulkan', description = 'Build Vulkan alongside OpenGL (requires Vulkan SDK)' }
PLAGC_VULKAN = _OPTIONS['vulkan'] ~= nil
local root = path.getabsolute('..')
local vk = root .. '/Vendor/ogre-next/RenderSystems/Vulkan'
local compiler = root .. '/Vendor/glslang'
local generated = root .. '/Bin-Int/Generated'
local sdk = os.getenv('VULKAN_SDK')
if PLAGC_VULKAN then
    assert(not _OPTIONS['null-platform'], '--vulkan cannot be combined with --null-platform')
    assert(os.target() ~= 'macosx', 'Vulkan integration currently supports Windows and Linux')
    if os.target() == 'windows' then
        assert(sdk and os.isfile(sdk .. '/Include/vulkan/vulkan.h') and os.isfile(sdk .. '/Lib/vulkan-1.lib'),
               'Install the Vulkan SDK and restart the terminal; VULKAN_SDK must contain Include and Lib')
    end
end
local function read(file)
    local f = assert(io.open(file, 'rb')); local s = f:read('*a'); f:close(); return s
end
local function write(file, data)
    os.mkdir(path.getdirectory(file))
    if os.isfile(file) and read(file) == data then return end
    local f = assert(io.open(file, 'wb')); f:write(data); f:close()
end
function vulkanIncludes()
    if not PLAGC_VULKAN then return end
    defines { 'PC_OGRE_VULKAN' }
    includedirs { vk .. '/include' }
    if sdk then
        filter 'system:windows'; includedirs { sdk .. '/Include' }
        filter 'system:linux'; includedirs { sdk .. '/include' }
        filter {}
    end
end
-- Keep the normal OpenGL-only engine source byte-for-byte unchanged. Vulkan builds use a
-- generated translation unit with the small hot-path changes required for explicit APIs.
function vulkanEngineSources()
    if not PLAGC_VULKAN then return end
    local original = root .. '/PlagC/src/Renderer/OgreRenderer.cpp'
    local patchRenderer = dofile(root .. '/Build/Patches/OgreRendererPerformance.lua')
    local output = generated .. '/PlagCOgreRenderer.cpp'
    write(output, patchRenderer(read(original)))
    removefiles { original }
    files { output }
end
function vulkanLibraries()
    if not PLAGC_VULKAN then return end
    local info = read(compiler .. '/build_info.h.tmpl'):gsub('@major@', '14'):gsub('@minor@', '3'):gsub('@patch@', '0'):gsub('@flavor@', '')
    write(generated .. '/glslang/build_info.h', info)
    project 'Glslang'
        kind 'StaticLib'; language 'C++'; cppdialect 'C++17'; warnings 'Off'; common()
        defines { 'ENABLE_OPT=0' }
        includedirs { compiler, generated }
        files { compiler .. '/glslang/GenericCodeGen/*.cpp', compiler .. '/glslang/MachineIndependent/*.cpp', compiler .. '/glslang/MachineIndependent/preprocessor/*.cpp' }
        for _,name in ipairs({'GlslangToSpv','InReadableOrder','Logger','SpvBuilder','SpvPostProcess','doc','SpvTools','disassemble'}) do
            files { compiler .. '/SPIRV/' .. name .. '.cpp' }
        end
        filter 'system:windows'; files { compiler .. '/glslang/OSDependent/Windows/ossource.cpp' }
        filter 'system:linux'; files { compiler .. '/glslang/OSDependent/Unix/ossource.cpp' }
        filter 'toolset:msc*'; buildoptions { '/bigobj' }
        filter {}
    -- Replace Ogre's ABI-sensitive local SpvOptions declaration with the pinned header.
    local original = vk .. '/src/OgreVulkanProgram.cpp'
    local source = read(original)
    local first = assert(source:find('// Inclusion of SPIRV headers', 1, true))
    local last, finish = source:find('}  // namespace glslang', first, true)
    assert(last, 'Pinned Ogre compiler integration changed')
    source = source:sub(1, first - 1) .. '#include "SPIRV/GlslangToSpv.h"\n' .. source:sub(finish + 1)
    source = source:gsub('glslang/SPIRV/Logger.h', 'SPIRV/Logger.h'):gsub('opts.disableOptimizer = false;', 'opts.disableOptimizer = true;'):gsub('opts.optimizeSize = true;', 'opts.optimizeSize = false;')
    write(generated .. '/OgreVulkanProgram.cpp', source)
    local xcb = vk .. '/src/Windowing/X11/OgreVulkanXcbSupport.cpp'
    local count
    source, count = read(xcb):gsub('optFullScreen.currentValue = "Yes";', 'optFullScreen.currentValue = mVideoModes.empty() ? "No" : "Yes";')
    assert(count == 1, 'Pinned Ogre XCB defaults changed')
    write(generated .. '/OgreVulkanXcbSupport.cpp', source)
    local allocator = vk .. '/src/Vao/OgreVulkanVaoManager.cpp'
    local patchMemory = dofile(root .. '/Build/Patches/VulkanMemory.lua')
    write(generated .. '/OgreVulkanVaoManager.cpp', patchMemory(read(allocator)))
    project 'OgreRenderVulkan'
        kind 'StaticLib'; language 'C++'; cppdialect 'C++17'; warnings 'Off'; common(); ogreIncludes()
        defines { 'ENABLE_OPT=0' }
        includedirs { compiler, generated, vk .. '/src' }
        files { vk .. '/src/*.cpp', vk .. '/src/Vao/*.cpp', vk .. '/src/SPIRV-Reflect/*.c' }
        removefiles { original, allocator }
        files { generated .. '/OgreVulkanProgram.cpp', generated .. '/OgreVulkanVaoManager.cpp' }
        filter 'system:windows'; files { vk .. '/src/Windowing/win32/*.cpp' }
        filter 'system:linux'
            files { vk .. '/src/Windowing/X11/*.cpp', generated .. '/OgreVulkanXcbSupport.cpp' }
            removefiles { xcb }
        filter 'toolset:msc*'; buildoptions { '/bigobj' }; defines { '_CRT_SECURE_NO_WARNINGS' }
        filter {}
        dependson { 'OgreMain', 'Glslang' }
end
function vulkanSystemLinks()
    if not PLAGC_VULKAN then return end
    filter 'system:windows'; links { 'vulkan-1' }; if sdk then libdirs { sdk .. '/Lib' } end
    filter 'system:linux'; links { 'vulkan', 'xcb-randr', 'X11-xcb' }
    if sdk then libdirs { sdk .. '/lib' } end
    filter {}
end
