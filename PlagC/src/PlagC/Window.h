#pragma once
#include "PlagC/Core.h"
#include "PlagC/Events/Event.h"

namespace PlagC
{

	struct WindowProps
	{
		std::string Title;
		unsigned int Width;
		unsigned int Height;
		WindowProps(const std::string& title = "PlagC Engine",
			unsigned int width = 1280,
			unsigned int height = 720)
			: Title(title), Width(width), Height(height) {}
	};

	class PLAGC_API Window
	{
	public:
		static Window* Create(const WindowProps& properties = WindowProps());

		using EventCallbackFn = std::function<void(Event&)>;
		
		virtual ~Window(){}

		virtual void OnUpdate() = 0;

		virtual unsigned int GetWidth() const = 0;
		virtual unsigned int GetHeight() const = 0;

		virtual void SetVsync(bool enabled) = 0;
		virtual bool IsVsync() const = 0;
		virtual void SetEventCallback(const EventCallbackFn& callback) = 0;
	};
}