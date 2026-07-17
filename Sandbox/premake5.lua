project "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir("../bin/" .. outputdir .. "/%{prj.name}")
    objdir("../bin-int/" .. outputdir .. "/%{prj.name}")


    files {
        "**.cpp",
        "**.h"
    }

    includedirs {
	"../PlagC/src",
	"../PlagC",
        "../PlagC/vendor/spdlog/include",
	"../PlagC/vendor/GLFW/include"
    }

    links {
        "PlagC"
    }

    defines {
        "PC_PLATFORM_WINDOWS",
        "PC_APP"
    }

    filter { "configurations:Debug" }
        buildoptions "/MDd"
        runtime "Debug"
        symbols "on"

    filter { "configurations:Release" }
        buildoptions "/MD"
        runtime "Release"
        optimize "on"