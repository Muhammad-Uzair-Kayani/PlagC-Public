#pragma once
#include "PlagC/Events/Event.h"
#include "PlagC/Events/ApplicationEvent.h"

namespace PlagC
{
	class PLAGC_API Application
	{
		public:
		Application();
		virtual ~Application();
		void Run();
		bool OnWindowClose(WindowResizeEvent& e);
	};

	

	Application* CreateApplication();
}


