#pragma once
#include "PlagC/Renderer/Buffer.h"

namespace PlagC
{
	class OpenGLVertexBuffer :
		public VertexBuffer
	{
	public:

		OpenGLVertexBuffer(float* vertices, uint32_t size);

		void Bind() const override;
		void UnBind() const override;

	private:
		uint32_t m_RendererID;
	};

	class OpenGLIndexBuffer :
		public IndexBuffer
	{
	public:

		OpenGLIndexBuffer(uint32_t* indices, uint32_t size, uint32_t count);

		void Bind() const override;
		void UnBind() const override;
		inline uint32_t GetCount() const override { return m_Count; }

	private:
		uint32_t m_RendererID;
		uint32_t m_Count;
	};

}


