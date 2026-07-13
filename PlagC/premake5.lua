project "PlagC"
    kind "SharedLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir("../bin/" .. outputdir .. "/%{prj.name}")
    objdir("../bin-int/" .. outputdir .. "/%{prj.name}")

    files {
        "**.cpp",
        "**.h"
    }

    postbuildcommands 
		{
			("echo Creating Sandbox folder if it does not exist..."),
			("if not exist \"%{wks.location}/bin/" .. outputdir .. "/Sandbox/\" mkdir \"%{wks.location}/bin/" .. outputdir .. "/Sandbox/\""),
			("echo Copying Hazel.dll to Sandbox..."),
			("{COPY} \"%{cfg.buildtarget.abspath}\" \"%{wks.location}/bin/" .. outputdir .. "/Sandbox/\"")
		}

    defines {
        "WINDOWS"
    }

    filter { "configurations:Debug" }
        buildoptions "/MTd"
        runtime "Debug"
        symbols "on"

    filter { "configurations:Release" }
        buildoptions "/MT"
        runtime "Release"
        optimize "on"