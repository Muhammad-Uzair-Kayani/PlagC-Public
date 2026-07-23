#include "pch.h"
#include "VertexArray.h"
#include "PlagC/Renderer/Renderer.h"
#include "Platfrom/OpenGL/OpenGLVertexArray.h"

namespace PlagC
{
	VertexArray* VertexArray::Create()
	{
		switch (Renderer::GetAPI())
		{
		case RendererAPI::None:
		{
			PC_CORE_ASSERT(false, "NONE API NOT SUPPORTED");
			return nullptr;
		}

		case RendererAPI::OpenGL:
		{
			PC_CORE_TRACE("OPENGL VERTEX ARRAY CREATED");
			return new OpenGLVertexArray();
		}

		default:
			break;
		}

		PC_CORE_ASSERT(false, "INVALID API NOT SUPPORTED");
		return nullptr;
	}
}
