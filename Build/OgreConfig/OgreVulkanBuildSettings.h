#pragma once
#define OGRE_VULKAN_WINDOW_NULL
#ifdef _WIN32
#define OGRE_VULKAN_WINDOW_WIN32
#elif defined(__linux__)
#define OGRE_VULKAN_WINDOW_XCB
#else
#error Unsupported Vulkan window platform
#endif
