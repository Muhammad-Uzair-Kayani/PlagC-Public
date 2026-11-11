newoption {
    trigger = "python-root",
    value = "PATH",
    description = "Enable the Python policy with a 64-bit CPython installation"
}

newoption {
    trigger = "pybind-include",
    value = "PATH",
    description = "Directory containing pybind11 headers"
}

local pythonRoot = _OPTIONS["python-root"]
local pybindInclude = _OPTIONS["pybind-include"]
if (pythonRoot == nil) ~= (pybindInclude == nil) then
    error("Supply both --python-root and --pybind-include to enable the Python policy.")
end

workspace "SurvivalGame"
    architecture "x86_64"
    configurations { "Debug", "Release" }
    location ("build/premake/" .. (_ACTION or "projects") .. (pythonRoot and "-python" or ""))
    startproject "GameRuntime"
    language "C++"
    cppdialect "C++20"
    targetdir "%{wks.location}/bin/%{cfg.buildcfg}"
    objdir "%{wks.location}/obj/%{cfg.buildcfg}/%{prj.name}"

    filter "configurations:Debug"
        symbols "On"
    filter "configurations:Release"
        optimize "On"
    filter {}

project "survival_core"
    kind "StaticLib"
    files { "cpp/core/include/**.hpp", "cpp/core/src/**.cpp" }
    includedirs { "cpp/core/include" }

project "GameRuntime"
    kind "ConsoleApp"
    files { "cpp/runtime/main.cpp", "cpp/runtime/transport.cpp", "cpp/runtime/transport.hpp" }
    includedirs { "cpp/core/include", "third_party" }
    links { "survival_core" }

if pythonRoot then
    pythonRoot = path.getabsolute(pythonRoot)
    pybindInclude = path.getabsolute(pybindInclude)
    if not os.isfile(path.join(pythonRoot, "include/Python.h")) or
       not os.isfile(path.join(pybindInclude, "pybind11/embed.h")) or
       #os.matchfiles(path.join(pythonRoot, "libs/python3*.lib")) == 0 then
        error("Expected a full Windows CPython installation and the pybind11 include directory.")
    end

    files { "cpp/runtime/python_policy.cpp", "cpp/runtime/python_policy.hpp" }
    includedirs { path.join(pythonRoot, "include"), pybindInclude }
    libdirs { path.join(pythonRoot, "libs") }
    defines {
        "SURVIVAL_ENABLE_PYTHON_PLUGIN",
        'SURVIVAL_PYTHON_MODULE_DIR="' .. path.translate(path.getabsolute("python"), "/") .. '"'
    }

    -- Match the CRT used by the standard Windows CPython distribution.
    filter "configurations:Debug"
        runtime "Release"
    filter {}
    project "survival_core"
        filter "configurations:Debug"
            runtime "Release"
        filter {}
end
