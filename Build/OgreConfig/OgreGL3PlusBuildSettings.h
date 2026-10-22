#pragma once
#ifdef _WIN32
#define OGRE_GLSUPPORT_USE_WGL 1
#elif defined(__APPLE__)
#define OGRE_GLSUPPORT_USE_COCOA 1
#else
#define OGRE_GLSUPPORT_USE_GLX 1
#define OGRE_GLSUPPORT_USE_EGL_HEADLESS 1
#endif
