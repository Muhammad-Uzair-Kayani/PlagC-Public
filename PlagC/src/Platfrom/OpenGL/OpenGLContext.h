#pragma once
#include "PlagC/Renderer/GraphicsContext.h"

struct GLFWwindow;

namespace PlagC
{
	class OpenGLContext : public GraphicsContext
	{

	public:

		OpenGLContext(GLFWwindow*);

		void Init() override;
		void SwapBuffers() override;

	private:

		GLFWwindow* m_WindowHandle;

	};
}


