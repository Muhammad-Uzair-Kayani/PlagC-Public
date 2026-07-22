#include "pch.h"
#include "OpenGLContext.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

PlagC::OpenGLContext::OpenGLContext(GLFWwindow* windowHandle) : m_WindowHandle(windowHandle)
{
	Init();
}

void PlagC::OpenGLContext::Init()
{
	glfwMakeContextCurrent(m_WindowHandle);

	//Initializing GLAD
	int status = gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
	PC_CORE_ASSERT(status, "Could not initialize GLFW!");
	if (status)
		PC_CORE_TRACE("SUCCESS IN LOADING GLAD ADRESS");
}

void PlagC::OpenGLContext::SwapBuffers()
{
	glfwSwapBuffers(m_WindowHandle);
}
