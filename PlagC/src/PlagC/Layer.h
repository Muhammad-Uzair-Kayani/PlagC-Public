#pragma once
#include "PlagC/Core.h"
#include "PlagC/Events/Event.h"
#include "PlagC/Core/Timestep.h"

namespace PlagC
{
	class PC_API Layer
	{
	public:

		Layer(std::string name = "Default Name") : DebugName(name) {}
		virtual ~Layer() {}

		virtual void OnUpdate(Timestep ts) {};
		virtual void OnEvent(Event& event) {};
		virtual void OnImGuiRender() {}
		virtual void OnAttach() {};
		virtual void OnDetach() {};

	private:

		const std::string DebugName;
	};
}


