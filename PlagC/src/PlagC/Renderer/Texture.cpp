#include "pch.h"
#include "Texture.h"

#include "Renderer.h"
#include "Platfrom/OpenGL/OpenGLTexture.h"

PlagC::Ref<PlagC::Texture2D> PlagC::Texture2D::Create(const std::string& path)
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
		return std::make_shared<OpenGLTexture2D>(path);
	}

	default:
		break;
	}

	PC_CORE_ASSERT(false, "INVALID API NOT SUPPORTED");
	return nullptr;
}
