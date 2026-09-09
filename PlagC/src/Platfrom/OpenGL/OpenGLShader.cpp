#include "pch.h"
#include "OpenGLShader.h"

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>


PlagC::OpenGLShader::OpenGLShader(const std::string& vertexSrc, const std::string& fragSrc)
{
	GLuint vertex, fragment;
	CreateShader(vertexSrc, GL_VERTEX_SHADER, vertex);
	CreateShader(fragSrc, GL_FRAGMENT_SHADER, fragment);
	CreateShaderProg(vertex, fragment);
	glDeleteShader(vertex);
	glDeleteShader(fragment);
}

void PlagC::OpenGLShader::Bind()
{
	glUseProgram(m_RendererID);
}

void PlagC::OpenGLShader::UnBind()
{
	glUseProgram(0);
}

void PlagC::OpenGLShader::CreateShader(std::string src, int type, unsigned int& shaderID)
{
	const char* source = src.c_str();
	shaderID = glCreateShader(type);
	glShaderSource(shaderID, 1, &source, NULL);
	glCompileShader(shaderID);

	int status;
	glGetShaderiv(shaderID, GL_COMPILE_STATUS, &status);

	if (!status)
	{
		GLint maxLength = 0;
		glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &maxLength);

		std::vector<GLchar> infoLog(maxLength);
		glGetShaderInfoLog(shaderID, maxLength, &maxLength, &infoLog[0]);

		// We don't need the shader anymore.
		glDeleteShader(shaderID);

		PC_CORE_ERROR("{0}", infoLog.data());
		PC_CORE_ASSERT(false, "Vertex shader compilation failure!");
	}
}

void PlagC::OpenGLShader::CreateShaderProg(unsigned int vertexShader, unsigned int fragmentShader)
{

	m_RendererID = glCreateProgram();
	glAttachShader(m_RendererID, vertexShader);
	glAttachShader(m_RendererID, fragmentShader);
	glLinkProgram(m_RendererID);

	GLint isLinked = 0;
	glGetProgramiv(m_RendererID, GL_LINK_STATUS, (int*)&isLinked);
	if (isLinked == GL_FALSE)
	{
		GLint maxLength = 0;
		glGetProgramiv(m_RendererID, GL_INFO_LOG_LENGTH, &maxLength);

		// The maxLength includes the NULL character
		std::vector<GLchar> infoLog(maxLength);
		glGetProgramInfoLog(m_RendererID, maxLength, &maxLength, &infoLog[0]);

		// We don't need the program anymore.
		glDeleteProgram(m_RendererID);
		// Don't leak shaders either.
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		PC_CORE_ERROR("{0}", infoLog.data());
		PC_CORE_ASSERT(false, "Shader link failure!");
		return;
	}
}

void PlagC::OpenGLShader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix)
{
	GLuint location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}


void PlagC::OpenGLShader::UploadUniformMat3(const std::string& name, const glm::mat3& matrix)
{
	GLuint location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}

void PlagC::OpenGLShader::UploadUniformFloat(const std::string& name, const float value)
{
	GLuint location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform1f(location, value);
}

void PlagC::OpenGLShader::UploadUniformFloat2(const std::string& name, const glm::vec2& values)
{
	GLuint location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform2f(location, values.x, values.y);
}

void PlagC::OpenGLShader::UploadUniformFloat3(const std::string& name, const glm::vec3& values)
{
	GLuint location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform3f(location, values.x, values.y, values.z);
}

void PlagC::OpenGLShader::UploadUniformFloat4(const std::string& name, const glm::vec4& values)
{
	GLuint location = glGetUniformLocation(m_RendererID, name.c_str());
	glUniform4f(location, values.x, values.y, values.z, values.w);
}

void PlagC::OpenGLShader::UploadUniformInt(const std::string& name, const int value)
{
	glUniform1i(glGetUniformLocation(m_RendererID, name.c_str()), value);
}
