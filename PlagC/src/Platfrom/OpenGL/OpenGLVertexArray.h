#pragma once
#include "PlagC/Renderer/VertexArray.h"


namespace PlagC
{
	class OpenGLVertexArray : public VertexArray
	{
	public:

		OpenGLVertexArray();
		virtual ~OpenGLVertexArray();

		void Bind() const override;
		void UnBind() const override;

		void AddVertexBuffer(const Ref<VertexBuffer>& vertexBuffer) override;
		void SetIndexBuffer(const Ref<IndexBuffer>& indexBuffer) override;

		const std::vector<Ref<VertexBuffer>>& GetVertexBuffers() const override
		{
			return m_VertexBuffer;
		}
		const Ref<IndexBuffer>& GetIndexBuffer() const override
		{
			return m_IndexBuffer;
		}

	private:

		std::vector<Ref<VertexBuffer>> m_VertexBuffer;
		Ref<IndexBuffer> m_IndexBuffer;
		uint32_t m_RendererID;
	};
}


