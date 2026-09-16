#include "pch.h"
#include "Application.h"
#include "PlagC/Core/Input.h"
#include "PlagC/Core/KeyCodes.h"
#include "PlagC/Renderer/Renderer.h"

#include <GLFW/glfw3.h>

namespace PlagC
{
	#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)
	Application* Application::s_Instance = nullptr;

}

PlagC::Application::Application()
{
	PC_PROFILE_FUNCTION();
	PC_CORE_ASSERT(!s_Instance, "APPLCATION INSTANCE ALREADY EXISTS");
	s_Instance = this;

	m_Window = std::unique_ptr<Window>(Window::Create());
	m_Window->SetEventCallback(BIND_EVENT_FN(OnEvent));
	m_Window->SetVsync(false);

	Renderer::Init();

	m_ImGuiLayer = new ImGuiLayer();
	PushOverLay(m_ImGuiLayer);

}

PlagC::Application::~Application()
{
	PC_PROFILE_FUNCTION();
	Renderer::Shutdown();
}

void PlagC::Application::OnEvent(Event& e)
{
	PC_PROFILE_FUNCTION();
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
	PC_PROFILE_FUNCTION();
	m_LayerStack.PushLayer(layer);
	layer->OnAttach();
}

void PlagC::Application::PushOverLay(Layer* layer)
{
	PC_PROFILE_FUNCTION();
	m_LayerStack.PushOverlay(layer);
	layer->OnAttach();
}

bool PlagC::Application::OnWindowClose(WindowCloseEvent& e)
{
	m_Running = false;
	return true;
}

bool PlagC::Application::OnWindowResize(WindowResizeEvent& e)
{
	PC_PROFILE_FUNCTION();
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
	PC_PROFILE_FUNCTION();
	while (m_Running)
	{
		PC_PROFILE_SCOPE("Application Run Loop");
		float time = glfwGetTime();
		Timestep timestep = (float)time - m_LastFrameTime;
		m_LastFrameTime = time;

		if (!m_Minimized)
		{
			PC_PROFILE_SCOPE("LayerStack OnUpdate");
			for (Layer* layer : m_LayerStack)
				layer->OnUpdate(timestep);

			{
				PC_PROFILE_SCOPE("LayerStack on ImGui-Render");
				m_ImGuiLayer->Begin();
				for (Layer* layer : m_LayerStack)
					layer->OnImGuiRender();
				m_ImGuiLayer->End();
			}
		}

		m_Window->OnUpdate();
	}
}




