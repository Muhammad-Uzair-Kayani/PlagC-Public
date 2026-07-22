#pragma once

namespace PlagC
{
	class Shader
	{
	public:

		Shader(std::string, std::string);

		void Bind();
		void UnBind();

	private:

		void CreateShader(std::string, int, unsigned int&);
		void CreateShaderProg(unsigned int, unsigned int);

	private:

		unsigned int m_RendererID;

	};
}

