#pragma once

#include "PlagC/Renderer/RendererAPI.h"
#include "glad/glad.h"

namespace PlagC
{
	class OpenGLRendererAPI :
		public RendererAPI
	{

	public:

		void Init() override;
		void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
		void SetClearColor(const glm::vec4& color) override;
		void Clear() override;

		void DrawIndexed(const Ref<VertexArray>& vertexArray) override;

	};
}


