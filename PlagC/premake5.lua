project "PlagC"
    kind "SharedLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    targetdir("../bin/" .. outputdir .. "/%{prj.name}")
    objdir("../bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "pch.h"
    pchsource "src/pch.cpp"
	
    files {
       "src/**.h",
	"src/**.cpp",
    }

    includedirs
    {
        "vendor/spdlog/include",
	"src",
        "vendor/GLFW/include",
        "vendor/Glad/include",
        "vendor/ImGui"
    }
	
    links
    {
        "GLFW",
	"opengl32.lib",
	"GLAD",
	"ImGui"
    }

    postbuildcommands 
		{
			("echo Creating Sandbox folder if it does not exist..."),
			("if not exist \"%{wks.location}/bin/" .. outputdir .. "/Sandbox/\" mkdir \"%{wks.location}/bin/" .. outputdir .. "/Sandbox/\""),
			("echo Copying Hazel.dll to Sandbox..."),
			("{COPY} \"%{cfg.buildtarget.abspath}\" \"%{wks.location}/bin/" .. outputdir .. "/Sandbox/\"")
		}

    defines {
        "PC_PLATFORM_WINDOWS",
        "PC_BUILD_DLL",
	"PC_ENABLE_ASSERTS"
    }

    filter { "configurations:Debug" }
        runtime "Debug"
        symbols "on"


    filter { "configurations:Release" }
        runtime "Release"
        optimize "on"