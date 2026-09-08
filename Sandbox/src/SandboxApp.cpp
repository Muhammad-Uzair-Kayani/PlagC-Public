#include <PlagC.h>
#include "imgui.h"

class ApplcationLayer : public PlagC::Layer
{

public:
	ApplcationLayer() :
		m_Camera(-1.6f, 1.6f, -0.9f, 0.9f), m_CameraPosition(0.0f)
	{
		PC_INFO("APPLICATION LAYER CREATED");

		m_VertexArray.reset(PlagC::VertexArray::Create());

		float vertices[3 * 7] = {
			-0.5f, -0.5f, 0.0f, 0.8f, 0.2f, 0.8f, 1.0f,
			 0.5f, -0.5f, 0.0f, 0.2f, 0.3f, 0.8f, 1.0f,
			 0.0f,  0.5f, 0.0f, 0.8f, 0.8f, 0.2f, 1.0f
		};

		std::shared_ptr<PlagC::VertexBuffer> vertexBuffer;
		vertexBuffer.reset(PlagC::VertexBuffer::Create(vertices, sizeof(vertices)));
		PlagC::BufferLayout layout = {
			{ PlagC::ShaderDataType::Float3, "a_Position" },
			{ PlagC::ShaderDataType::Float4, "a_Color" }
		};
		vertexBuffer->SetLayout(layout);
		m_VertexArray->AddVertexBuffer(vertexBuffer);

		uint32_t indices[3] = { 0, 1, 2 };
		std::shared_ptr<PlagC::IndexBuffer> indexBuffer;
		indexBuffer.reset(PlagC::IndexBuffer::Create(indices, sizeof(indices) / sizeof(uint32_t)));
		m_VertexArray->SetIndexBuffer(indexBuffer);

		m_SquareVA.reset(PlagC::VertexArray::Create());

		float squareVertices[3 * 4] = {
			-0.75f, -0.75f, 0.0f,
			 0.75f, -0.75f, 0.0f,
			 0.75f,  0.75f, 0.0f,
			-0.75f,  0.75f, 0.0f
		};

		std::shared_ptr<PlagC::VertexBuffer> squareVB;
		squareVB.reset(PlagC::VertexBuffer::Create(squareVertices, sizeof(squareVertices)));
		squareVB->SetLayout({
			{ PlagC::ShaderDataType::Float3, "a_Position" }
			});
		m_SquareVA->AddVertexBuffer(squareVB);

		uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };
		std::shared_ptr<PlagC::IndexBuffer> squareIB;
		squareIB.reset(PlagC::IndexBuffer::Create(squareIndices, sizeof(squareIndices) / sizeof(uint32_t)));
		m_SquareVA->SetIndexBuffer(squareIB);

		std::string vertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;
			layout(location = 1) in vec4 a_Color;

			out vec3 v_Position;
			out vec4 v_Color;

			uniform mat4 u_ViewProjection;

			void main()
			{
				v_Position = a_Position;
				v_Color = a_Color;
				gl_Position = u_ViewProjection * vec4(a_Position, 1.0);	
			}
		)";

		std::string fragmentSrc = R"(
			#version 330 core
			
			layout(location = 0) out vec4 color;

			in vec3 v_Position;
			in vec4 v_Color;

			void main()
			{
				color = vec4(v_Position * 0.5 + 0.5, 1.0);
				color = v_Color;
			}
		)";

		m_Shader.reset(new PlagC::Shader(vertexSrc, fragmentSrc));

		std::string blueShaderVertexSrc = R"(
			#version 330 core
			
			layout(location = 0) in vec3 a_Position;

			out vec3 v_Position;

			uniform mat4 u_ViewProjection;

			void main()
			{
				v_Position = a_Position;
				gl_Position = u_ViewProjection * vec4(a_Position, 1.0);	
			}
		)";

		std::string blueShaderFragmentSrc = R"(
			#version 330 core
			
			layout(location = 0) out vec4 color;

			in vec3 v_Position;

			void main()
			{
				color = vec4(0.2, 0.3, 0.8, 1.0);
			}
		)";

		m_BlueShader.reset(new PlagC::Shader(blueShaderVertexSrc, blueShaderFragmentSrc));

	}
	~ApplcationLayer() {}

	void OnImGuiRender()
	{
		ImGui::Begin("Test");
		ImGui::Text("Hello World");
		ImGui::End();
	}

	void OnUpdate(PlagC::Timestep ts) override
	{
		//TESTING PURPOSES
		//START

		PC_TRACE("Delta Time: {0}s ({1}s)", ts, ts.GetMiliSeconds());

		if (PlagC::Input::IsKeyPressed(PC_KEY_LEFT))
			m_CameraPosition.x += m_CameraMoveSpeed * ts;
		else if (PlagC::Input::IsKeyPressed(PC_KEY_RIGHT))
			m_CameraPosition.x -= m_CameraMoveSpeed * ts;

		if (PlagC::Input::IsKeyPressed(PC_KEY_UP))
			m_CameraPosition.y -= m_CameraMoveSpeed * ts;
		else if (PlagC::Input::IsKeyPressed(PC_KEY_DOWN))
			m_CameraPosition.y += m_CameraMoveSpeed * ts;

		if (PlagC::Input::IsKeyPressed(PC_KEY_A))
			m_CameraRotation -= m_CameraRotationSpeed * ts;
		if (PlagC::Input::IsKeyPressed(PC_KEY_D))
			m_CameraRotation += m_CameraRotationSpeed * ts;

		PlagC::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		PlagC::RenderCommand::Clear();

		m_Camera.SetPosition(m_CameraPosition);
		m_Camera.SetRotation(m_CameraRotation);
		//END

		PlagC::Renderer::BeginScene(m_Camera);

		PlagC::Renderer::Submit(m_BlueShader, m_SquareVA);
		PlagC::Renderer::Submit(m_Shader, m_VertexArray);

		PlagC::Renderer::EndScene();
	}
	void OnEvent(PlagC::Event& e) override
	{

	}

private:

	std::shared_ptr<PlagC::Shader> m_Shader;
	std::shared_ptr<PlagC::Shader> m_BlueShader;

	std::shared_ptr<PlagC::VertexArray> m_VertexArray;
	std::shared_ptr<PlagC::VertexArray> m_SquareVA;

	std::shared_ptr<PlagC::VertexBuffer> m_VertexBuffer;
	std::shared_ptr<PlagC::IndexBuffer> m_IndexBuffer;

	PlagC::OrthographicCamera m_Camera;
	glm::vec3 m_CameraPosition;
	float m_CameraMoveSpeed = 0.1f;

	float m_CameraRotation = 0.0f;
	float m_CameraRotationSpeed = 1.f;

};

class Sandbox : public PlagC::Application
{
public:
	Sandbox()
	{
		// Initialize your application here
		PushLayer(new ApplcationLayer());

	}
	~Sandbox()
	{
		// Clean up your application here
	}
};
 
PlagC::Application* PlagC::CreateApplication()
{
	return new Sandbox();
}