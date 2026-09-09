#pragma once
#include "PlagC/Renderer/Shader.h"

namespace PlagC
{
	class OpenGLShader : public Shader
	{
	public:

		OpenGLShader(const std::string& vertexSource, const std::string& fragmentSource);
		virtual ~OpenGLShader() = default;

		void Bind() override;
		void UnBind() override;

		void UploadUniformMat4(const std::string& name, const glm::mat4& matrix);
		void UploadUniformMat3(const std::string& name, const glm::mat3& matrix);

		void UploadUniformFloat(const std::string& name, const float value);
		void UploadUniformFloat2(const std::string& name, const glm::vec2& values);
		void UploadUniformFloat3(const std::string& name, const glm::vec3& values);
		void UploadUniformFloat4(const std::string& name, const glm::vec4& values);

		void UploadUniformInt(const std::string& name, const int value);

	private:

		void CreateShader(std::string, int, unsigned int&);
		void CreateShaderProg(unsigned int, unsigned int);

	private:

		unsigned int m_RendererID;

	};
}


