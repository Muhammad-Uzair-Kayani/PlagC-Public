include "Build/VerifyDependencies.lua"
newoption { trigger = "null-platform", description = "Build GLFW Null backend only (headless validation)" }
newoption { trigger = "sanitize", description = "Enable Linux sanitizers", value = "KIND", allowed = { {"address", "Address + undefined behavior"}, {"thread", "Thread sanitizer"} } }
local root = path.getabsolute(".")
workspace "PlagC"
    location "Build/Generated"
    configurations { "Debug", "Release", "Dist" }
    architecture "x86_64"
    startproject "Sandbox"
    flags { "MultiProcessorCompile" }
    filter "system:windows"
        systemversion "latest"
        filter { "system:windows", "language:C++" }
            buildoptions { "/utf-8", "/permissive-", "/Zc:__cplusplus" }
    filter "configurations:Debug"
        symbols "On"
        defines { "PC_DEBUG", "PC_ENABLE_PROFILING" }
        runtime "Debug"
    filter "configurations:Release"
        optimize "Speed"
        symbols "On"
        defines { "NDEBUG", "PC_ENABLE_PROFILING" }
        runtime "Release"
        filter { "configurations:Release", "toolset:msc*" }
            buildoptions { "/O2" }
    filter "configurations:Dist"
        optimize "Speed"
        symbols "Off"
        defines { "NDEBUG", "PC_DIST" }
        runtime "Release"
        filter { "configurations:Dist", "toolset:msc*" }
            buildoptions { "/O2" }
    filter {}
    if _OPTIONS["sanitize"] then
        filter "system:linux"
            local flag = _OPTIONS["sanitize"] == "address" and "-fsanitize=address,undefined" or "-fsanitize=thread"
            buildoptions { flag, "-fno-omit-frame-pointer" }
            linkoptions { flag }
        filter {}
    end
function common()
    local suffix = (_OPTIONS["null-platform"] and "-null" or "") .. (_OPTIONS["sanitize"] and ("-" .. _OPTIONS["sanitize"] or ""))
    targetdir (root .. "/Bin" .. suffix .. "/%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}")
    objdir (root .. "/Bin-Int" .. suffix .. "/%{cfg.system}-%{cfg.architecture}/%{cfg.buildcfg}/%{prj.name}")
end
-- Each target builds its own PCH with its own flags and definitions.
function enginePch(source)
    pchheader "pgcpch.h"
    pchsource(source)
    includedirs { root .. "/Build/Pch" }
    files { root .. "/Build/Pch/pgcpch.h" }
end
include "Build/Dependencies.lua"
project "RuntimeAssets"
    kind "Utility"
    common()
    files { "Scripts/StageRuntimeAssets.py" }
    filter "files:Scripts/StageRuntimeAssets.py"
        buildmessage "Verifying and staging runtime assets"
        buildcommands { 'python "' .. root .. '/Scripts/StageRuntimeAssets.py" --root "' .. root .. '" --output "%{cfg.targetdir}" --sentinel "%{cfg.objdir}/runtime-assets.always"' }
        -- The staging script touches this only after verification succeeds.
        buildoutputs { "%{cfg.objdir}/runtime-assets.always" }
    filter {}
project "PlagC"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    common()
    warnings "Extra"
    filter "toolset:msc*"
        disablewarnings { "4324" }
    filter {}
    files { "PlagC/include/**.h", "PlagC/src/**.h", "PlagC/src/**.cpp" }
    removefiles { "PlagC/src/Assets/AssimpImporter.cpp", "PlagC/src/Assets/WaveImporter.cpp" }
    assetRuntimeIncludes()
    dependson { "Ozz", "OpenAL" }
    enginePch("PlagC/src/pgcpch.cpp")
    includedirs { "PlagC/include", "Vendor/glfw/include", "Vendor/spdlog/include", "Vendor/entt/src" }
    dependson { "EnTT", "Jolt" }
    defines { "SPDLOG_COMPILED_LIB" }
    ogreIncludes()
    joltIncludes()
    dependson { "GLFW", "spdlog", "OgreMain", "OgreHlmsCommon", "OgreHlmsPbs", "OgreHlmsUnlit", "OgreRenderNULL" }
    if not _OPTIONS["null-platform"] then dependson { "OgreRenderGL3Plus" } end
project "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    common()
    warnings "Extra"
    filter "toolset:msc*"
        disablewarnings { "4324" }
    filter {}
    files {
        "Sandbox/src/**.cpp", "Sandbox/src/**.h",
        "Game/src/**.cpp", "Game/src/**.h",
        "Game/Framework/**.cpp", "Game/Framework/**.h",
        "Game/Default/**.cpp", "Game/Default/**.h",
        "Game/SurvivalGame/**.cpp", "Game/SurvivalGame/**.h"
    }
    enginePch("Sandbox/src/pgcpch.cpp")
    includedirs {
        "PlagC/include",
        "Game/src",
        "Game/Framework",
        "Game/Default",
        "Game/SurvivalGame"
    }
    links { "PlagC", "GLFW", "spdlog", "Jolt", "Ozz", "OpenAL" }
    filter "system:linux"
        linkoptions { "-Wl,-rpath,'$$ORIGIN'" }
    filter {}
    ogreLinks()
    ogreSystemLinks()
    debugdir "%{cfg.targetdir}"
    dependson { "RuntimeAssets" }
    filter "system:windows"
        links { "gdi32", "user32", "shell32", "psapi" }
    filter { "system:windows", "configurations:Dist" }
        kind "WindowedApp"
        entrypoint "mainCRTStartup"
        -- Dist is the distributable package. All engine/vendor code is linked
        -- into Sandbox.exe; keep runtime data but remove linker/debug artifacts.
        postbuildcommands {
            'python "' .. root .. '/Scripts/PruneDistOutput.py" --output "%{cfg.targetdir}"'
        }
    filter "system:linux"
        links { "pthread", "dl", "m" }
    filter "system:macosx"
        links { "Cocoa.framework", "IOKit.framework", "CoreFoundation.framework" }
    filter "files:Game/**.cpp"
        defines { "PC_TPS_VISUAL_MAX" }
    filter {}

project "AssetCooker"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    common()
    files { "Tools/AssetCooker/**.cpp", "Tools/AssetCooker/**.h" }
    includedirs { "PlagC/include", "Build/Pch" }
    links { "PlagCImport", "PlagC", "Assimp" }
    debugdir "%{wks.location}/../.."
    filter "system:linux"
        links { "pthread" }
    filter {}
