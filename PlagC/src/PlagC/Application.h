#pragma once
#include "Core.h"

namespace PlagC
{
	class PLAGC_API Application
	{
		public:
		Application();
		virtual ~Application();
		void Run();

	};

	Application* CreateApplication();
}


