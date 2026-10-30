-- Paths are relative to this file; vendor sources remain untouched.
project "spdlog"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    common()
    warnings "Off"
    files { "../Vendor/spdlog/src/*.cpp" }
    includedirs { "../Vendor/spdlog/include" }
    defines { "SPDLOG_COMPILED_LIB" }
