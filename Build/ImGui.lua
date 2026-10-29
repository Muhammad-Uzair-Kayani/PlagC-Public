-- Dear ImGui core plus GLFW platform backend. Rendering stays engine-owned.
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
        "../Vendor/imgui/imgui_widgets.cpp",
        "../Vendor/imgui/backends/imgui_impl_glfw.cpp"
    }
    includedirs {
        "../Vendor/imgui",
        "../Vendor/glfw/include"
    }
    defines { "IMGUI_DISABLE_OBSOLETE_FUNCTIONS" }
    dependson { "GLFW" }
