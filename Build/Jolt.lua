-- Exact source revision lives in the parent gitlink. CPU-only, static, SSE2 baseline.
-- Keep ABI-affecting definitions identical in Jolt and its PlagC adapter.
local joltRoot = path.getabsolute("../Vendor/jolt", _SCRIPT_DIR)
function joltIncludes()
    includedirs { joltRoot }
    defines { "JPH_DISABLE_CUSTOM_ALLOCATOR" }
end
project "Jolt"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    common()
    files { "../Vendor/jolt/Jolt/**.cpp", "../Vendor/jolt/Jolt/**.h", "../Vendor/jolt/Jolt/**.inl" }
    removefiles { "../Vendor/jolt/Jolt/ObjectStream/ObjectStream*.cpp", "../Vendor/jolt/Jolt/ObjectStream/TypeDeclarations.cpp" }
    -- SerializableObject.cpp supplies RTTI even when object streaming is disabled.
    includedirs { "../Vendor/jolt" }
    defines { "JPH_DISABLE_CUSTOM_ALLOCATOR" }
    -- Premake beta2 accepts Default/Fast/Strict, but not Precise.
    floatingpoint "Default"
    filter "toolset:msc*"
        buildoptions { "/fp:precise" }
    filter {}
    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS", "NOMINMAX" }
    filter {}
