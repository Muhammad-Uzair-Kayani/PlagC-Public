#include "pch.h"
#include "Application.h"

#include <GLFW/glfw3.h>

namespace PlagC
{
	#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)
	Application* Application::s_Instance = nullptr;
}

PlagC::Application::Application()
{
	PC_CORE_ASSERT(!s_Instance, "APPLCATION INSTANCE ALREADY EXISTS");
	s_Instance = this;
	m_Window = std::unique_ptr<Window>(Window::Create());
	m_Window->SetEventCallback(BIND_EVENT_FN(OnEvent));
}

PlagC::Application::~Application()
{

}

void PlagC::Application::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));

	PC_CORE_INFO(e.ToString() + " Event Dispatched");

	for (auto it = m_LayerStack.end(); it != m_LayerStack.end(); it++)
	{
		(*--it)->OnEvent(e);
		if (e.Handled)
			break;
	}
}

void PlagC::Application::PushLayer(Layer* layer)
{
	m_LayerStack.PushLayer(layer);
	layer->OnAttach();
}

void PlagC::Application::PushOverLay(Layer* layer)
{
	m_LayerStack.PopOverlay(layer);
	layer->OnAttach();
}

bool PlagC::Application::OnWindowClose(WindowCloseEvent& e)
{
	m_Running = false;
	return true;
}

void PlagC::Application::Run()
{

	while (m_Running)
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(0.1f, 0.1f, 0.4f, 1.0f);

		for (Layer* layer : m_LayerStack)
			layer->OnUpdate();

		m_Window->OnUpdate();
	}
}




