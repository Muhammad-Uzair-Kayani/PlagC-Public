#pragma once
#include "PlagC/Events/Event.h"
#include "PlagC/Events/ApplicationEvent.h"
#include "PlagC/Window.h"
#include "LayerStack.h"
#include "PlagC/ImGui/ImGuiLayer.h"
#include "Platfrom/OpenGL/Shader.h"
#include "PlagC/Renderer/Buffer.h"
#include "PlagC/Renderer/VertexArray.h"
#include "PlagC/Renderer/OrthographicCamera.h"

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
		ImGuiLayer* m_ImGuiLayer;

	private:

		std::shared_ptr<Shader> m_Shader;
		std::shared_ptr<Shader> m_BlueShader;

		std::shared_ptr<VertexArray> m_VertexArray;
		std::shared_ptr<VertexArray> m_SquareVA;
		
		std::shared_ptr<VertexBuffer> m_VertexBuffer;
		std::shared_ptr<IndexBuffer> m_IndexBuffer;

		OrthographicCamera m_Camera;

	private:

		static Application* s_Instance;

	};


	Application* CreateApplication();
}


