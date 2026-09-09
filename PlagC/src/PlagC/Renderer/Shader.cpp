#include "pch.h"
#include "Shader.h"

#include "PlagC/Renderer/Renderer.h"
#include "Platfrom/OpenGL/OpenGLShader.h"

namespace PlagC
{
	Shader* Shader::Create(const std::string& vertexSrc, const std::string& fragmentSrc)
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::API::None:
		{
			PC_CORE_ASSERT(false, "NONE API NOT SUPPORTED");
			return nullptr;
		}

		case RendererAPI::API::OpenGL:
		{
			PC_CORE_TRACE("OPENGL VERTEX ARRAY CREATED");
			return new OpenGLShader(vertexSrc, fragmentSrc);
		}

		default:
			break;
		}

		PC_CORE_ASSERT(false, "INVALID API NOT SUPPORTED");
		return nullptr;
	}
}
