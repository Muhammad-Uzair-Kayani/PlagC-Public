#pragma once
#include "PlagC/Window.h"

#include <GLFW/glfw3.h>

namespace PlagC
{

	class WindowsWindow : public Window
	{

	public:

		WindowsWindow(const WindowProps& properties);
		virtual ~WindowsWindow();

		virtual void OnUpdate();

		virtual inline unsigned int GetWidth() const override { return m_Data.Width; }
		virtual inline unsigned int GetHeight() const override { return m_Data.Height; }

		virtual void SetVsync(bool enabled) override;
		virtual bool IsVsync() const override;

		static void GLFWErrorCallback(int error, const char* description);

		virtual inline void SetEventCallback(const EventCallbackFn& callback) { m_Data.EventCallback = callback; }


	private:

		virtual void Init(const WindowProps& properties);
		virtual void OnShutdown();

	private:

		GLFWwindow* m_Window;
		static bool s_GLFWInitialized;

		struct WindowData
		{
			std::string Title;
			unsigned int Width, Height;
			bool Vsync;
			EventCallbackFn EventCallback;
		};

		WindowData m_Data;

	};

}

