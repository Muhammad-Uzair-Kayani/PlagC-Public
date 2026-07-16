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

#define BIT(x) (1 << x)