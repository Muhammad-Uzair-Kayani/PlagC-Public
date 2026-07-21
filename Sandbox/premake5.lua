project "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

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
	"../PlagC/vendor/GLFW/include",
	"../PlagC/vendor/glm",
		"../PlagC/vendor/ImGui"
    }

    links {
        "PlagC"
    }

    defines {
        "PC_PLATFORM_WINDOWS",
        "PC_APP"
    }

    filter { "configurations:Debug" }
        runtime "Debug"
        symbols "on"

    filter { "configurations:Release" }
        runtime "Release"
        optimize "on"