#pragma once
#include "PlagC/Renderer/Texture.h"


namespace PlagC
{
	class OpenGLTexture2D :
		public Texture2D
	{
	public:

		OpenGLTexture2D(const std::string& path);

		virtual ~OpenGLTexture2D();

		const uint32_t GetWidth() const override { return m_Width; }
		const uint32_t GetHeight() const override { return m_Height; }

		void Bind(uint32_t slot = 0) const override;

	private:

		std::string m_Path;
		uint32_t m_Width = 0, m_Height = 0;
		uint32_t m_RendererID = 0;

	};
}


