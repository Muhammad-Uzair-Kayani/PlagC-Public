#include "pch.h"
#include "WindowsInput.h"
#include "PlagC/Application.h"

#include <GLFW/glfw3.h>

PlagC::Input* PlagC::Input::s_Instance = new PlagC::WindowsInput();

bool PlagC::WindowsInput::IsKeyPressedImpl(int keycode)
{
	auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	auto state = glfwGetKey(window, keycode);
	return state == GLFW_PRESS || state == GLFW_REPEAT;
}

bool PlagC::WindowsInput::IsMouseButtonPressedImpl(int button)
{
	auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	auto state = glfwGetKey(window, button);
	return state == GLFW_PRESS;
}

std::pair<float, float> PlagC::WindowsInput::GetMousePositionImpl()
{
	auto window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
	double Xpos, Ypos;
	glfwGetCursorPos(window, &Xpos, &Ypos);
	return { (float)Xpos, (float)Ypos };
}

float PlagC::WindowsInput::GetMouseXImpl()
{
	return GetMousePositionImpl().first;
}

float PlagC::WindowsInput::GetMouseYImpl()
{
	return GetMousePositionImpl().second;
}
