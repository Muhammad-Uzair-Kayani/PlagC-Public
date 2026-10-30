-- Pinned-source build manifests are shared with Scripts/BuildAssetVendors.py.
-- No vendor CMake build or moving-tag fetch occurs here.
local assetRoot=path.getabsolute("..",_SCRIPT_DIR)
local manifest=json.decode(io.readfile(path.join(assetRoot,"Build/AssetVendors.json")))
for _,name in ipairs({"Assimp","Ozz","OpenAL"}) do
    local config=manifest[name]
    project(name)
        kind "StaticLib"
        language "C++"
        cppdialect "C++20"
        warnings "Off"
        common()
        for _,file in ipairs(config.sources) do files { path.join(assetRoot,file) } end
        for _,dir in ipairs(config.includes) do includedirs { path.join(assetRoot,dir) } end
        defines(config.defines)
        filter "system:windows"
            defines { "_CRT_SECURE_NO_WARNINGS", "NOMINMAX" }
        filter {}
    if name=="OpenAL" then
        -- OpenAL Soft's public headers otherwise declare the API as dllimport on
        -- Windows. Keep this define on every target that includes those headers.
        defines { "AL_LIBTYPE_STATIC" }
        filter "system:windows"
            defines { "HAVE_WINMM=1", "_WIN32_WINNT=0x0601", "_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR" }
            files { path.join(assetRoot,"Vendor/openal/alc/backends/winmm.cpp") }
            links { "winmm", "ole32", "shell32", "user32" }
        filter "system:linux"
            defines { "HAVE_OSS=1" }
            files { path.join(assetRoot,"Vendor/openal/alc/backends/oss.cpp") }
            links { "pthread", "dl" }
        filter "system:macosx"
            defines { "HAVE_COREAUDIO=1" }
            files { path.join(assetRoot,"Vendor/openal/alc/backends/coreaudio.cpp") }
            links { "AudioToolbox.framework", "CoreAudio.framework", "CoreFoundation.framework", "AudioUnit.framework", "ApplicationServices.framework" }
        filter {}
    end
end
function assetRuntimeIncludes()
    includedirs {
        path.join(assetRoot,"Vendor/ozz/include"),
        path.join(assetRoot,"Vendor/openal/include"),
        -- PlagC's texture importer uses the stb_image copy pinned with Assimp.
        path.join(assetRoot,"Vendor/assimp/contrib")
    }
    -- Required by OpenAL Soft when consumers include AL/al.h and AL/alc.h while
    -- linking the implementation statically (notably avoids __declspec(dllimport)).
    defines { "AL_LIBTYPE_STATIC" }
end
project "PlagCImport"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    common()
    files { "../PlagC/src/Assets/AssimpImporter.cpp", "../PlagC/src/Assets/WaveImporter.cpp" }
    includedirs { "../PlagC/include", "Pch", "Config/Assimp", "../Vendor/assimp/include", "../Vendor/assimp/contrib" }
    dependson { "Assimp" }
    filter "system:windows"
        defines { "_CRT_SECURE_NO_WARNINGS", "NOMINMAX" }
    filter {}
