#pragma once
#include "glm/glm.hpp"

namespace PlagC
{
	class Shader
	{
	public:

		Shader(std::string, std::string);

		void Bind();
		void UnBind();

		void UploadUnifromMat4(const std::string& name, const glm::mat4& matrix);

	private:

		void CreateShader(std::string, int, unsigned int&);
		void CreateShaderProg(unsigned int, unsigned int);

	private:

		unsigned int m_RendererID;

	};
}

