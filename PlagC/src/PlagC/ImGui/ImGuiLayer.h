#pragma once

#include "PlagC/Layer.h"
#include "PlagC/Events/ApplicationEvent.h"
#include "PlagC/Events/KeyEvent.h"
#include "PlagC/Events/MouseEvent.h"

namespace PlagC
{
	class PC_API ImGuiLayer : public  Layer
	{
	public:

		ImGuiLayer();
		~ImGuiLayer();

		void OnUpdate() override;
		void OnEvent(Event& event) override;
		void OnAttach() override;
		void OnDetach() override;

		bool OnMouseButtonPressedEvent(MouseButtonPressedEvent& e);
		bool OnMouseButtonReleasedEvent(MouseButtonReleasedEvent& e);
		bool OnMouseMovedEvent(MouseMovedEvent& e);
		bool OnMouseScrolledEvent(MouseScrolledEvent& e);
		bool OnKeyPressedEvent(KeyPressedEvent& e);
		bool OnKeyReleasedEvent(KeyReleasedEvent& e);
		bool OnKeyTypedEvent(KeyTypedEvent& e);
		bool OnWindowResizeEvent(WindowResizeEvent& e);


		void Begin();
		void End();

		void SetDarkThemeColors();

		void DrawDockspace();
		void DrawMenuBar();
		void DrawSceneHierarchy();
		void DrawViewport();
		void DrawInspector();
		void DrawConsole();
		void DrawStatistics();


	private:

		float m_Time = 0.0f;

	};

}
