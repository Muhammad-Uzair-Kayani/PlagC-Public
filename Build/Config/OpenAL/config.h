#pragma once
#define FORCE_ALIGN
#define HAVE_RTKIT 0
#define ALSOFT_UWP 0
#define ALSOFT_EAX 0
#ifdef _WIN32
#define HAVE_INTRIN_H
#define HAVE_GUIDDEF_H
#define HAVE_CPUID_INTRINSIC
#else
#define HAVE_DLFCN_H
#define HAVE_PTHREAD_SETSCHEDPARAM
#if defined(__x86_64__)
#define HAVE_CPUID_H
#define HAVE_GCC_GET_CPUID
#endif
#endif
