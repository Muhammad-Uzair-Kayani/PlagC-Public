local base=path.getabsolute('../Vendor/ogre-next')
local config=path.getabsolute('OgreConfig')
local json=path.getabsolute('../Vendor/rapidjson/include')
function ogreIncludes()
    includedirs {config,base..'/OgreMain/include',json,base..'/Components/Hlms/Common/include',base..'/Components/Hlms/Pbs/include',base..'/Components/Hlms/Unlit/include',base..'/RenderSystems/NULL/include',base..'/RenderSystems/GL3Plus/include',base..'/RenderSystems/GL3Plus/include/GLSL',base..'/RenderSystems/GL3Plus/include/GL'}
    if _OPTIONS['null-platform'] then defines {'PC_OGRE_NULL_ONLY'} end
    filter 'system:windows'
        includedirs {base..'/OgreMain/include/WIN32'}
        defines {'NOMINMAX'}
        -- This Ogre revision uses narrow strings with generic Win32 APIs.
        characterset 'MBCS'
    filter 'system:linux'
        includedirs {base..'/OgreMain/include/GLX'}
    filter 'system:macosx'
        includedirs {base..'/OgreMain/include/OSX'}
    filter {}
end
local function library(name)
    project(name)
    kind 'StaticLib'
    language 'C++'
    cppdialect 'C++17'
    warnings 'Off'
    common()
    ogreIncludes()
    filter 'toolset:msc*'
        buildoptions {'/bigobj'}
        defines {'_CRT_SECURE_NO_WARNINGS'}
    filter {}
end
project 'rapidjson'
    kind 'Utility'
    files {json..'/**.h'}
library('OgreMain')
    defines {'OGRE_NONCLIENT_BUILD'}
    dependson {'rapidjson'}
    files {base..'/OgreMain/src/*.cpp'}
    for _,name in ipairs({'OgreFreeImageCodec2','OgreZip','OgreAPKZipArchive','OgrePVRTCCodec','OgreETCCodec','OgreASTCCodec','OgreSearchOps'}) do removefiles {base..'/OgreMain/src/'..name..'.cpp'} end
    for _,dir in ipairs({'Animation','CommandBuffer','Compositor','Compute','Hash','ogrestd','Vao'}) do files {base..'/OgreMain/src/'..dir..'/**.cpp'} end
    files {base..'/OgreMain/src/Math/Array/*.cpp',base..'/OgreMain/src/Math/Array/SSE2/Single/*.cpp',base..'/OgreMain/src/Math/Simple/*.cpp',base..'/OgreMain/src/Math/Simple/C/*.cpp',base..'/OgreMain/src/Threading/OgreDefaultWorkQueueStandard.cpp',base..'/OgreMain/src/Threading/OgreSemaphore.cpp',base..'/OgreMain/src/Threading/OgreWaitableEvent.cpp'}
    filter 'system:windows'
        files {base..'/OgreMain/src/WIN32/*.cpp',base..'/OgreMain/src/WIN32/OgreWin32Resources.rc',base..'/OgreMain/src/Threading/*Win.cpp'}
        removefiles {base..'/OgreMain/src/WIN32/*WinRT.cpp'}
    filter {'system:windows', 'toolset:msc*'}
        -- MinGW-only CRT shims conflict with MSVC's real security cookie.
        removefiles {base..'/OgreMain/src/WIN32/OgreMinGWSupport.cpp'}
    filter 'system:linux'
        files {base..'/OgreMain/src/GLX/OgreTimer.cpp',base..'/OgreMain/src/GLX/OgreErrorDialog.cpp',base..'/OgreMain/src/GLX/OgreFileSystemLayer.cpp',base..'/OgreMain/src/OgreSearchOps.cpp',base..'/OgreMain/src/Threading/*PThreads.cpp'}
    filter 'system:macosx'
        files {base..'/OgreMain/src/OSX/*.cpp',base..'/OgreMain/src/OSX/*.mm',base..'/OgreMain/src/OgreSearchOps.cpp',base..'/OgreMain/src/Threading/*PThreads.cpp'}
    filter {}
library('OgreHlmsCommon')
    files {base..'/Components/Hlms/Common/src/*.cpp'}
library('OgreHlmsPbs')
    files {base..'/Components/Hlms/Pbs/src/**.cpp'}
    defines {'OgreHlmsPbs_EXPORTS'}
    dependson {'OgreHlmsCommon','OgreMain'}
library('OgreHlmsUnlit')
    files {base..'/Components/Hlms/Unlit/src/*.cpp'}
    defines {'OgreHlmsUnlit_EXPORTS'}
    dependson {'OgreHlmsCommon','OgreMain'}
library('OgreRenderNULL')
    files {base..'/RenderSystems/NULL/src/**.cpp'}
    dependson {'OgreMain'}
if not _OPTIONS['null-platform'] then
    library('OgreRenderGL3Plus')
    local gl=base..'/RenderSystems/GL3Plus'
    files {gl..'/src/*.cpp',gl..'/src/Vao/*.cpp',gl..'/src/GLSL/*.cpp'}
    -- Keep the submodule immutable. Generate one patched translation unit at setup.
    local original = gl .. '/src/GLSL/OgreGLSLMonolithicProgram.cpp'
    local input = assert(io.open(original, "rb")); local source = input:read("*a"); input:close()
    local marker = "OGRE_CHECK_GL_ERROR( glValidateProgram( mGLProgramHandle ) );"
    local first = assert(source:find(marker, 1, true), "Pinned Ogre GLSL validation site changed")
    assert(not source:find(marker, first + #marker, true), "Ambiguous Ogre GLSL validation site")
    local patched = source:sub(1, first - 1) .. '#include "PlagCGLSLSamplerInitialization.inl"\n            ' .. source:sub(first)
    local generated = path.getabsolute("../Bin-Int/Generated/OgreGLSLMonolithicProgram.cpp")
    os.mkdir(path.getdirectory(generated))
    local existing = io.open(generated, "rb")
    local previous = existing and existing:read("*a") or nil
    if existing then existing:close() end
    if previous ~= patched then local out = assert(io.open(generated, "wb")); out:write(patched); out:close() end
    removefiles { original }
    files { generated }
    includedirs { path.getabsolute("Patches") }

    includedirs {gl..'/include/GLSL',gl..'/include/GL'}
    filter 'system:linux'
        files {gl..'/src/windowing/OgreGlSwitchableSupport.cpp',gl..'/src/windowing/EGL/PBuffer/*.cpp'}
        includedirs {gl..'/src/windowing',gl..'/include/windowing/EGL'}
    filter {}
    for _,entry in ipairs({{'windows','win32','cpp'},{'linux','GLX','cpp'},{'macosx','OSX','mm'}}) do
        filter('system:'..entry[1])
        files {gl..'/src/windowing/'..entry[2]..'/*.'..entry[3]}
        includedirs {gl..'/include/windowing/'..entry[2],gl..'/src/windowing/'..entry[2]}
    end
    filter {}
    dependson {'OgreMain'}
end
function ogreLinks()
    links {'OgreHlmsPbs','OgreHlmsUnlit','OgreHlmsCommon','OgreRenderNULL'}
    if not _OPTIONS['null-platform'] then links {'OgreRenderGL3Plus'} end
    links {'OgreMain'}
end
function ogreSystemLinks()
    filter 'system:windows'
        links {'opengl32','gdi32','user32','shell32','advapi32','ole32'}
    filter 'system:linux'
        links {'pthread','dl','m','atomic','X11','xcb'}
        if not _OPTIONS['null-platform'] then links {'GL','EGL','Xrandr'} end
    filter 'system:macosx'
        links {'Cocoa.framework','IOKit.framework','CoreFoundation.framework','Carbon.framework','OpenGL.framework','CoreVideo.framework'}
    filter {}
end
