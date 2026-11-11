-- Persistent Python subprocess: no Python headers, .lib files or pybind11.
newoption {
    trigger = "python-exe",
    value = "PATH",
    description = "Python 3.10+ executable used by the Visual Studio debugger"
}
local root = path.getabsolute(".")
local pythonExe = _OPTIONS["python-exe"]
if not pythonExe then
    local probes = os.host() == "windows" and { "py -3", "python", "python3" }
        or { "python3", "python" }
    for _, probe in ipairs(probes) do
        local output = os.outputof(probe .. ' -c "import sys; print(sys.executable)"')
        local candidate = output and output:match("^%s*(.-)%s*$")
        if candidate and os.isfile(candidate) then
            pythonExe = candidate
            break
        end
    end
end
pythonExe = pythonExe or (os.host() == "windows" and "python" or "python3")
print("GameRuntime Python interpreter: " .. pythonExe)
workspace "SurvivalGame"
    architecture "x86_64"
    configurations { "Debug", "Release" }
    location ("build/premake/" .. (_ACTION or "projects"))
    startproject "GameRuntime"
    language "C++"
    cppdialect "C++20"
    targetdir "%{wks.location}/bin/%{cfg.buildcfg}"
    objdir "%{wks.location}/obj/%{cfg.buildcfg}/%{prj.name}"
    debugdir (root)
    filter "system:windows"
        systemversion "latest"
    filter "configurations:Debug"
        symbols "On"
    filter "configurations:Release"
        optimize "On"
    filter {}
project "survival_core"
    kind "StaticLib"
    files { "cpp/core/include/**.hpp", "cpp/core/src/game_session.cpp", "cpp/core/src/simulation.cpp" }
    includedirs { "cpp/core/include" }
project "survival_synthetic"
    kind "StaticLib"
    files { "cpp/core/src/synthetic.cpp" }
    includedirs { "cpp/core/include" }
    links { "survival_core" }
project "GameRuntime"
    kind "ConsoleApp"
    files {
        "cpp/runtime/main.cpp", "cpp/runtime/transport.cpp", "cpp/runtime/transport.hpp",
        "python/neural.py", "python/service.py", "python/train.py"
    }
    includedirs { "cpp/core/include", "third_party" }
    links { "survival_synthetic", "survival_core" }
    debugargs {
        '--python', '"' .. pythonExe .. '"',
        '--policy-service', '"' .. path.join(root, "python/service.py") .. '"',
        '--model', '"' .. path.join(root, "docs/samples/champion.json") .. '"'
    }
project "core_tests"
    kind "ConsoleApp"
    files { "cpp/tests/core_tests.cpp" }
    includedirs { "cpp/core/include" }
    links { "survival_core" }
project "scenario_tests"
    kind "ConsoleApp"
    files { "cpp/tests/scenario_tests.cpp" }
    includedirs { "cpp/core/include" }
    links { "survival_synthetic", "survival_core" }
