#pragma once
#include "PlagC/Renderer/Texture.h"
#include <glad/glad.h>


namespace PlagC
{
	class OpenGLTexture2D :
		public Texture2D
	{
	public:

		OpenGLTexture2D(const std::string& path);
		OpenGLTexture2D(uint32_t width, uint32_t heigth);

		virtual ~OpenGLTexture2D();

		const uint32_t GetWidth() const override { return m_Width; }
		const uint32_t GetHeight() const override { return m_Height; }

		void SetData(void* data, uint32_t size) override;

		void Bind(uint32_t slot = 0) const override;

	private:

		std::string m_Path;
		uint32_t m_Width = 0, m_Height = 0;
		uint32_t m_RendererID = 0;

		GLenum m_InternalFormat, m_DataFormat;
	};
}


