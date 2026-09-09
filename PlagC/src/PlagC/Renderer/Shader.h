#pragma once
#include "glm/glm.hpp"

namespace PlagC
{
	class Shader
	{
	public:

		virtual void Bind() = 0;
		virtual void UnBind() = 0;

		static Shader* Create(const std::string& vertexSource, const std::string& fragmentSrouce);

	private:

	};
}

