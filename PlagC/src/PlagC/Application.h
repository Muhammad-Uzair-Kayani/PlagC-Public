#pragma once
#include "PlagC/Events/Event.h"
#include "PlagC/Events/ApplicationEvent.h"
#include "PlagC/Window.h"
#include "LayerStack.h"


namespace PlagC
{
	class PC_API Application
	{
		public:
		Application();
		virtual ~Application();

		void Run();
		void OnEvent(Event& e);
		void PushLayer(Layer* layer);
		void PushOverLay(Layer* Layer);

		static inline Application& Get() { return *s_Instance; }
		inline Window& GetWindow() { return *m_Window; }

	private:
		
		bool OnWindowClose(WindowCloseEvent& e);

	private:

		std::unique_ptr<Window> m_Window;
		bool m_Running = true;
		LayerStack m_LayerStack;

	private:

		static Application* s_Instance;

	};


	Application* CreateApplication();
}


