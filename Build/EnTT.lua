-- EnTT is header-only; no vendor CMake or compiled archive is needed.
project "EnTT"
    kind "Utility"
    language "C++"
    files { "../Vendor/entt/src/entt/**.hpp" }
