workspace "PlagC"
    architecture "x64"

    configurations {
        "Debug",
        "Release"
    }

    startproject "Sandbox"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

include "PlagC/vendor/Glad"
include "PlagC/vendor/GLFW"
include "PlagC"
include "Sandbox"