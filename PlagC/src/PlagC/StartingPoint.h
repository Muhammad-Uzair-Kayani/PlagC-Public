#pragma once

#include "log.h"

#ifdef PC_PLATFORM_WINDOWS
	

extern PlagC::Application* PlagC::CreateApplication();

int main(int argc, char** argv)
{
	PlagC::log::Init();
	PC_CORE_INFO("Initializing PlagC Engine");

	PlagC::Application* app = PlagC::CreateApplication();
	app->Run();
	delete app;
}


#else 
	#error PlagC only supports Windows!
#endif