project "GLFW"
    kind "StaticLib"
    language "C"
    cdialect "C99"
    common()
    warnings "Off"
    includedirs { "../Vendor/glfw/include", "../Vendor/glfw/src" }
    files {
        "../Vendor/glfw/src/context.c", "../Vendor/glfw/src/init.c", "../Vendor/glfw/src/input.c",
        "../Vendor/glfw/src/monitor.c", "../Vendor/glfw/src/platform.c", "../Vendor/glfw/src/vulkan.c",
        "../Vendor/glfw/src/window.c", "../Vendor/glfw/src/egl_context.c", "../Vendor/glfw/src/osmesa_context.c",
        "../Vendor/glfw/src/null_init.c", "../Vendor/glfw/src/null_monitor.c", "../Vendor/glfw/src/null_window.c", "../Vendor/glfw/src/null_joystick.c"
    }
    filter "system:windows"
        files { "../Vendor/glfw/src/win32_module.c", "../Vendor/glfw/src/win32_time.c", "../Vendor/glfw/src/win32_thread.c" }
    filter "system:linux"
        files { "../Vendor/glfw/src/posix_module.c", "../Vendor/glfw/src/posix_time.c", "../Vendor/glfw/src/posix_thread.c" }
        defines { "_DEFAULT_SOURCE" }
    filter "system:macosx"
        files { "../Vendor/glfw/src/posix_module.c", "../Vendor/glfw/src/cocoa_time.c", "../Vendor/glfw/src/posix_thread.c" }
    filter {}
    if not _OPTIONS["null-platform"] then
        filter "system:windows"
            defines { "_GLFW_WIN32" }
            files { "../Vendor/glfw/src/win32_init.c", "../Vendor/glfw/src/win32_monitor.c", "../Vendor/glfw/src/win32_window.c", "../Vendor/glfw/src/win32_joystick.c", "../Vendor/glfw/src/wgl_context.c" }
        filter "system:linux"
            defines { "_GLFW_X11" }
            files { "../Vendor/glfw/src/x11_init.c", "../Vendor/glfw/src/x11_monitor.c", "../Vendor/glfw/src/x11_window.c", "../Vendor/glfw/src/glx_context.c", "../Vendor/glfw/src/xkb_unicode.c", "../Vendor/glfw/src/linux_joystick.c", "../Vendor/glfw/src/posix_poll.c" }
        filter "system:macosx"
            defines { "_GLFW_COCOA" }
            files { "../Vendor/glfw/src/cocoa_init.m", "../Vendor/glfw/src/cocoa_joystick.m", "../Vendor/glfw/src/cocoa_monitor.m", "../Vendor/glfw/src/cocoa_window.m", "../Vendor/glfw/src/nsgl_context.m" }
        filter {}
    end
