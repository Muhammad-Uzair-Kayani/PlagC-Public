#pragma once
#ifdef PC_PLATFORM_WINDOWS

	#ifdef PC_BUILD_DLL
		#define PLAGC_API __declspec(dllexport)
	#else
		#define PLAGC_API __declspec(dllimport)
	#endif
#else 
	//#error PlagC only supports Windows!

#endif // PC_PLATFORM_WINDOWS

#ifdef PC_ENABLE_ASSERTS
	#define PLAGC_ASSERT(x, ...) { if(!(x)) { PC_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
	#define PLAGC_CORE_ASSERT(x, ...) { if(!(x)) { PC_CORE_ERROR("Assertion Failed: {0}", __VA_ARGS__); __debugbreak(); } }
#else
	#define PLAGC_ASSERT(x, ...)
	#define PLAGC_CORE_ASSERT(x, ...)
#endif

#define BIT(x) (1 << x)