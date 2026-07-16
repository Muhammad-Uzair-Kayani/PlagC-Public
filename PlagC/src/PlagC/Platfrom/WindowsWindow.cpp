#include "pch.h"
#include "WindowsWindow.h"

#include "PlagC/Events/ApplicationEvent.h"
#include "PlagC/Events/KeyEvent.h"
#include "PlagC/Events/MouseEvent.h"

bool PlagC::WindowsWindow::s_GLFWInitialized = false;

PlagC::WindowsWindow::WindowsWindow(const WindowProps& properties)
{
	Init(properties);
}

PlagC::WindowsWindow::~WindowsWindow()
{
	OnShutdown();
}

void PlagC::WindowsWindow::OnUpdate()
{
	glfwPollEvents();
	glfwSwapBuffers(m_Window);
}

void PlagC::WindowsWindow::SetVsync(bool enabled)
{
	if(enabled)
		glfwSwapInterval(1);
	else
		glfwSwapInterval(0);
	m_Data.Vsync = enabled;
}

bool PlagC::WindowsWindow::IsVsync() const
{
	return m_Data.Vsync;
}



void PlagC::WindowsWindow::GLFWErrorCallback(int error, const char* description)
{
	PC_CORE_ERROR("GLFW Error ({0}): {1}", error, description);
}

PlagC::Window* PlagC::Window::Create(const WindowProps& properties)
{
	return new WindowsWindow(properties);
}

void PlagC::WindowsWindow::Init(const WindowProps& properties)
{
	//GLFW initializations
	m_Data.Title = properties.Title;
	m_Data.Width = properties.Width;
	m_Data.Height = properties.Height;

	PC_CORE_INFO("Trying to create window {0} ({1}, {2})", properties.Title, properties.Width, properties.Height);

	if(!s_GLFWInitialized)
	{
		int success = glfwInit();
		PLAGC_CORE_ASSERT(success, "Could not initialize GLFW!");
		glfwSetErrorCallback(GLFWErrorCallback);
		s_GLFWInitialized = true;
	}

	PC_CORE_INFO("GLFW initialized successfully! Creating WIN");

	m_Window = glfwCreateWindow((int)properties.Width,
		(int)properties.Height, properties.Title.c_str(), nullptr, nullptr);
	glfwMakeContextCurrent(m_Window);
	glfwSetWindowUserPointer(m_Window, &m_Data);
	SetVsync(1);

	//Set GLFW callbacks
	glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height)
	{
		WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
		data.Width = width;
		data.Height = height;
		WindowResizeEvent event(width, height);
		data.EventCallback(event);
	});

	glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			WindowCloseEvent event;
			data.EventCallback(event);
	});

	glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
			WindowCloseEvent event;
			data.EventCallback(event);
	});

	glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			switch (action)
			{
			case GLFW_PRESS:
			{
				KeyPressedEvent event(key, 0);
				data.EventCallback(event);
				break;
			}
			case GLFW_RELEASE:
			{
				KeyReleasedEvent event(key);
				data.EventCallback(event);
				break;
			}
			case GLFW_REPEAT:
			{
				KeyPressedEvent event(key, 1);
				data.EventCallback(event);
				break;
			}
			}
		});

	glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			switch (action)
			{
			case GLFW_PRESS:
			{
				MouseButtonPressedEvent event(button);
				data.EventCallback(event);
				break;
			}
			case GLFW_RELEASE:
			{
				MouseButtonReleasedEvent event(button);
				data.EventCallback(event);
				break;
			}
			}
	});

	glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			MouseScrolledEvent event((float)xOffset, (float)yOffset);
			data.EventCallback(event);
	});

	glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos)
		{
			WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

			MouseMovedEvent event((float)xPos, (float)yPos);
			data.EventCallback(event);
	});
}

void PlagC::WindowsWindow::OnShutdown()
{
	//GLFW shutdown/Destroy window
	glfwDestroyWindow(m_Window);
}
