#include "pch.h"
#include "Application.h"
#include "PlagC/Input.h"
#include "PlagC/KeyCodes.h"
#include "Platfrom/OpenGL/Shader.h"

#include <glad/glad.h>
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

	m_ImGuiLayer = new ImGuiLayer();
	m_LayerStack.PushLayer(m_ImGuiLayer);

	glGenVertexArrays(1, &m_VertexArray);
	glBindVertexArray(m_VertexArray);


	float vertices[] = {
		0.0, 0.5, 0.5,
		0.5, -0.5, 0.5,
		-0.5, -0.5, 0.5
	};
	m_VertexBuffer.reset(VertexBuffer::Create(vertices, sizeof(vertices)));
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
	glEnableVertexAttribArray(0);


	uint32_t indices[] = { 0, 1, 2 };
	m_IndexBuffer.reset(IndexBuffer::Create(indices, sizeof(indices), 3));

	std::string vertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;

			out vec3 v_Position;

			void main()
			{
				v_Position = a_Position;
				gl_Position = vec4(a_Position, 1.0);	
			}
		)";

	std::string fragmentSrc = R"(
			#version 330 core
			
			layout(location = 0) out vec4 color;

			in vec3 v_Position;

			void main()
			{
				color = vec4(v_Position * 0.5 + 0.5, 1.0);
			}
		)";

	m_Shader = std::make_unique<Shader>(vertexSrc, fragmentSrc);
}

PlagC::Application::~Application()
{

}

void PlagC::Application::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<WindowCloseEvent>(BIND_EVENT_FN(OnWindowClose));

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

void PlagC::Application::Run()
{

	while (m_Running)
	{
		glClear(GL_COLOR_BUFFER_BIT);
		glClearColor(0.1f, 0.1f, 0.4f, 1.0f);

		m_Shader->Bind();
		glBindVertexArray(m_VertexArray);
		glDrawElements(GL_TRIANGLES, m_IndexBuffer->GetCount(), GL_UNSIGNED_INT, nullptr);

		for (Layer* layer : m_LayerStack)
			layer->OnUpdate();

		m_ImGuiLayer->Begin();
		for (Layer* layer : m_LayerStack)
			layer->OnImGuiRender();
		m_ImGuiLayer->End();

		m_Window->OnUpdate();
	}
}




