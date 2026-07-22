#include "pch.h"
#include "Buffer.h"
#include "PlagC/Renderer/Renderer.h"
#include "Platfrom/OpenGL/OpenGLBuffer.h"



namespace PlagC
{
	VertexBuffer* VertexBuffer::Create(float* vertices, uint32_t size)
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
			PC_CORE_TRACE("OPENGL VERTEX BUFFER CREATED");
			return new OpenGLVertexBuffer(vertices, size);
		}

		default:
			break;
		}

		PC_CORE_ASSERT(false, "INVALID API NOT SUPPORTED");
		return nullptr;
	}

	IndexBuffer* IndexBuffer::Create(uint32_t* indices, uint32_t size, uint32_t count)
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
			PC_CORE_TRACE("OPENGL VERTEX BUFFER CREATED");
			return new OpenGLIndexBuffer(indices, size, count);
		}

		default:
			break;
		}

		PC_CORE_ASSERT(false, "INVALID API NOT SUPPORTED");
		return nullptr;
	}
}
