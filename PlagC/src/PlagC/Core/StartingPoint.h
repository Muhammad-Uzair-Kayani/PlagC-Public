#pragma once

#include "log.h"

#ifdef PC_PLATFORM_WINDOWS
	

extern PlagC::Application* PlagC::CreateApplication();

int main(int argc, char** argv)
{
	PlagC::log::Init();
	

	PC_PROFILE_BEGIN_SESSION("Startup", "PlagCProfile-Startup.json");
	auto app = PlagC::CreateApplication();
	PC_PROFILE_END_SESSION();

	PC_PROFILE_BEGIN_SESSION("Runtime", "PlagCProfile-Runtime.json");
	app->Run();
	PC_PROFILE_END_SESSION();

	PC_PROFILE_BEGIN_SESSION("Startup", "PlagCProfile-Shutdown.json");
	delete app;
	PC_PROFILE_END_SESSION();
}


#else 
	#error PlagC only supports Windows!
#endif