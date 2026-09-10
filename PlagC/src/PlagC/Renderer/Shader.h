#pragma once
#include "glm/glm.hpp"

namespace PlagC
{
	class Shader
	{
	public:

		virtual void Bind() const = 0;
		virtual void UnBind() const = 0;

		static Ref<Shader> Create(const std::string& filepath);
		static Ref<Shader> Create(const std::string& name, const std::string& vertexSource, const std::string& fragmentSrouce);

		virtual const std::string& GetName() const = 0;

	private:

	};

	class ShaderLibrary
	{
	public:

		void Add(const std::string& name, const Ref<Shader>& shader);
		void Add(const Ref<Shader>& shader);
		Ref<Shader> Load(const std::string& filepath);
		Ref<Shader> Load(const std::string& name, const std::string& filepath);

		Ref<Shader> Get(const std::string& name);

		bool Exists(const std::string& name) const;

	private:

		std::unordered_map<std::string, Ref<Shader>> m_Shaders;

	};
}

