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
		void OnAttach() override;
		void OnDetach() override;
		void OnImGuiRender();

		void Begin();
		void End();

	private:

		float m_Time = 0.0f;

	};

}
