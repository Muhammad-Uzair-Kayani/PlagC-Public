#include "pch.h"
#include "RenderCommand.h"
#include "Platfrom/OpenGL/OpenGLRendererAPI.h"

namespace PlagC
{
	RendererAPI* RenderCommand::s_RendererAPI = new OpenGLRendererAPI;
}