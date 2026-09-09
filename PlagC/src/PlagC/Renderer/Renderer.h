#pragma once
#include "RenderCommand.h";
#include "PlagC/Renderer/OrthographicCamera.h"
#include "PlagC/Renderer/Shader.h"

namespace PlagC
{
	class Renderer
	{
	public:

		static void BeginScene(OrthographicCamera& camera);
		static void EndScene();
		static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform);

		inline static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }

	private:

		struct SceneData
		{
			glm::mat4 ViewProjectionMatrix;
		};

		static SceneData* s_SceneData;
	};

}



