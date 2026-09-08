#pragma once

#include "PlagC/Renderer/RendererAPI.h"
#include "glad/glad.h"

namespace PlagC
{
	class OpenGLRendererAPI :
		public RendererAPI
	{

	public:
		void SetClearColor(const glm::vec4& color) override;
		void Clear() override;

		void DrawIndexed(const std::shared_ptr<VertexArray>& vertexArray) override;

	};
}


