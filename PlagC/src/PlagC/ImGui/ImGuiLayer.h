#pragma once

#include "PlagC/Layer.h"

namespace PlagC
{
	class ImGuiLayer : public  Layer
	{
	public:

		ImGuiLayer();
		~ImGuiLayer();

		void OnUpdate();
		void OnEvent(Event& event);
		void OnAttach();
		void OnDetach();

	private:


	};

}

