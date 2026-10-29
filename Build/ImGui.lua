-- Dear ImGui core. Platform input and rendering stay engine-owned so the editor
-- does not leak GLFW or a concrete graphics API into its frontend architecture.
project "ImGui"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    common()
    warnings "Off"
    files {
        "../Vendor/imgui/imgui.cpp",
        "../Vendor/imgui/imgui_draw.cpp",
        "../Vendor/imgui/imgui_tables.cpp",
        "../Vendor/imgui/imgui_widgets.cpp"
    }
    includedirs { "../Vendor/imgui" }
    defines { "IMGUI_DISABLE_OBSOLETE_FUNCTIONS" }
