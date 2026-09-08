#include "pch.h"
#include "RendererCommand.h"
#include "Platfrom/OpenGL/OpenGLRendererAPI.h"

namespace PlagC
{
	RendererAPI* RendererCommand::s_RendererAPI = new OpenGLRendererAPI;
}