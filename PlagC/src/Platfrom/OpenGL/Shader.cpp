#include "pch.h"
#include "Shader.h"

#include <glad/glad.h>


PlagC::Shader::Shader(std::string vertexSrc, std::string fragSrc)
{
	GLuint vertex, fragment;
	CreateShader(vertexSrc, GL_VERTEX_SHADER, vertex);
	CreateShader(fragSrc, GL_FRAGMENT_SHADER, fragment);
	CreateShaderProg(vertex, fragment);
	glDeleteShader(vertex);
	glDeleteShader(fragment);
}

void PlagC::Shader::Bind()
{
	glUseProgram(m_RendererID);
}

void PlagC::Shader::UnBind()
{
	glUseProgram(0);
}


void PlagC::Shader::CreateShader(std::string src, int type, unsigned int& shaderID)
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

void PlagC::Shader::CreateShaderProg(unsigned int vertexShader, unsigned int fragmentShader)
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
