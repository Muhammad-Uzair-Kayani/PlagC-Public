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
        defines { "PC_DEBUG" }
        runtime "Debug"
    filter "configurations:Release"
        optimize "Speed"
        symbols "On"
        defines { "NDEBUG" }
        runtime "Release"
    filter "configurations:Dist"
        optimize "Full"
        symbols "Off"
        defines { "NDEBUG", "PC_DIST" }
        runtime "Release"
    filter {}
    if _OPTIONS["sanitize"] then
        filter "system:linux"
            local flag = _OPTIONS["sanitize"] == "address" and "-fsanitize=address,undefined" or "-fsanitize=thread"
            buildoptions { flag, "-fno-omit-frame-pointer" }
            linkoptions { flag }
        filter {}
    end
function common()
    local suffix = (_OPTIONS["null-platform"] and "-null" or "") .. (_OPTIONS["sanitize"] and ("-" .. _OPTIONS["sanitize"]) or "")
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
project "PlagC"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    common()
    warnings "Extra"
    files { "PlagC/include/**.h", "PlagC/src/**.cpp" }
    enginePch("PlagC/src/pgcpch.cpp")
    includedirs { "PlagC/include", "Vendor/glfw/include", "Vendor/spdlog/include" }
    defines { "SPDLOG_COMPILED_LIB" }
    ogreIncludes()
    ogreLinks()
    links { "GLFW", "spdlog" }
project "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    common()
    warnings "Extra"
    files { "Sandbox/src/**.cpp", "Sandbox/src/**.h" }
    enginePch("Sandbox/src/pgcpch.cpp")
    includedirs { "PlagC/include" }
    links { "PlagC", "GLFW", "spdlog" }
    ogreLinks()
    debugdir "%{wks.location}/../.."
    filter "system:windows"
        links { "gdi32", "user32", "shell32" }
    filter "system:linux"
        links { "pthread", "dl", "m" }
    filter "system:macosx"
        links { "Cocoa.framework", "IOKit.framework", "CoreFoundation.framework" }
    filter {}
