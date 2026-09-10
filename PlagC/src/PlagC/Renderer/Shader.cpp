#include "pch.h"
#include "Shader.h"

#include "PlagC/Renderer/Renderer.h"
#include "Platfrom/OpenGL/OpenGLShader.h"

namespace PlagC
{

	Ref<Shader> Shader::Create(const std::string& filepath)
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
			return std::make_shared<OpenGLShader>(filepath);
		}

		default:
			break;
		}

		PC_CORE_ASSERT(false, "INVALID API NOT SUPPORTED");
		return nullptr;
	}


	Ref<Shader> Shader::Create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
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
			return std::make_shared<OpenGLShader>(name, vertexSrc, fragmentSrc);
		}

		default:
			break;
		}

		PC_CORE_ASSERT(false, "INVALID API NOT SUPPORTED");
		return nullptr;
	}

	

	void ShaderLibrary::Add(const std::string& name, const Ref<Shader>& shader)
	{
		PC_CORE_ASSERT(!Exists(name), "Shader Already Exist");
		m_Shaders[name] = shader;
	}

	void ShaderLibrary::Add(const Ref<Shader>& shader)
	{
		auto name = shader->GetName();
		Add(name, shader);
	}

	PlagC::Ref<PlagC::Shader> ShaderLibrary::Load(const std::string& filepath)
	{
		auto shader = Shader::Create(filepath);
		Add(shader);
		return shader;
	}

	PlagC::Ref<PlagC::Shader> ShaderLibrary::Load(const std::string& name, const std::string& filepath)
	{
		auto shader = Shader::Create(filepath);
		Add(name, shader);
		return shader;
	}

	PlagC::Ref<PlagC::Shader> ShaderLibrary::Get(const std::string& name)
	{
		return m_Shaders[name];
	}

	bool ShaderLibrary::Exists(const std::string& name) const
	{
		return m_Shaders.find(name) != m_Shaders.end();
	}

}
