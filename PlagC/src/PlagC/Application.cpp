#include "pch.h"
#include "Application.h"
#include "PlagC/Input.h"
#include "PlagC/KeyCodes.h"
#include "PlagC/Renderer/Renderer.h"

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
	m_Window->SetVsync(false);
	m_LastFrameTime = glfwGetTime();

	Renderer::Init();

	m_ImGuiLayer = new ImGuiLayer();
	m_LayerStack.PushLayer(m_ImGuiLayer);

}

PlagC::Application::~Application()
{

}

void PlagC::Application::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));
	dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(OnWindowResize));

	for (auto it = m_LayerStack.end(); it != m_LayerStack.begin(); )
	{
		(*--it)->OnEvent(e);

		if (e.Handled)
			break;
	}
}

void PlagC::Application::PushLayer(Layer* layer)
{
	m_LayerStack.PushLayer(layer);
}

void PlagC::Application::PushOverLay(Layer* layer)
{
	m_LayerStack.PushOverlay(layer);
}

bool PlagC::Application::OnWindowClose(WindowCloseEvent& e)
{
	m_Running = false;
	return true;
}

bool PlagC::Application::OnWindowResize(WindowResizeEvent& e)
{
	if (e.GetWidth() == 0 || e.GetHeight() == 0)
	{
		m_Minimized = true;
		return false;
	}

	m_Minimized = false;
	Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());
	return false;
}

void PlagC::Application::Run()
{

	while (m_Running)
	{

		float time = glfwGetTime();
		Timestep timestep = time - m_LastFrameTime;
		m_LastFrameTime = time;

		if (!m_Minimized)
		{
			for (Layer* layer : m_LayerStack)
				layer->OnUpdate(timestep);
		}

		m_ImGuiLayer->Begin();
		for (Layer* layer : m_LayerStack)
			layer->OnImGuiRender();
		m_ImGuiLayer->End();

		m_Window->OnUpdate();
	}
}




