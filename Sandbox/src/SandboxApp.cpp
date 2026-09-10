#include <PlagC.h>
#include "imgui.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class ApplcationLayer : public PlagC::Layer
{

public:
	ApplcationLayer() :
		m_Camera(-1.6f, 1.6f, -0.9f, 0.9f), m_CameraPosition(0.0f)
	{
		PC_INFO("APPLICATION LAYER CREATED");

		m_SquareVA.reset(PlagC::VertexArray::Create());

		float squareVertices[5 * 4] = {
			-0.5f, -0.5f, 0.0f, 0.0f, 0.0f,
			 0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
			 0.5f,  0.5f, 0.0f, 1.0f, 1.0f,
			-0.5f,  0.5f, 0.0f, 0.0f, 1.0f
		};

		PlagC::Ref<PlagC::VertexBuffer> squareVB;
		squareVB.reset(PlagC::VertexBuffer::Create(squareVertices, sizeof(squareVertices)));
		squareVB->SetLayout({
			{ PlagC::ShaderDataType::Float3, "a_Position" },
			{ PlagC::ShaderDataType::Float2, "a_TexCoord" }
			});
		m_SquareVA->AddVertexBuffer(squareVB);

		uint32_t squareIndices[6] = { 0, 1, 2, 2, 3, 0 };
		PlagC::Ref<PlagC::IndexBuffer> squareIB;
		squareIB.reset(PlagC::IndexBuffer::Create(squareIndices, sizeof(squareIndices) / sizeof(uint32_t)));
		m_SquareVA->SetIndexBuffer(squareIB);

		m_Texture = PlagC::Texture2D::Create("assets/textures/Checkerboard.png");
		m_LogoTexture = PlagC::Texture2D::Create("assets/textures/Logo.png");

		auto textureshader = m_ShaderLibrary.Load("assets/shaders/texture.glsl");
		auto colorshader = m_ShaderLibrary.Load("assets/shaders/color.glsl");
		std::dynamic_pointer_cast<PlagC::OpenGLShader>(textureshader)->Bind();
		std::dynamic_pointer_cast<PlagC::OpenGLShader>(textureshader)->UploadUniformInt("u_Texture", 0);
	}
	~ApplcationLayer() {}

	void OnImGuiRender()
	{
		ImGui::Begin("Colors Settings");
		ImGui::ColorEdit3("Square Color", glm::value_ptr(m_SquareColor));
		ImGui::End();
	}

	void OnUpdate(PlagC::Timestep ts) override
	{
		//TESTING PURPOSES
		//START

		PC_TRACE("Delta Time: {0}s ({1}s)", ts, ts.GetMiliSeconds());

		if (PlagC::Input::IsKeyPressed(PC_KEY_LEFT))
			m_CameraPosition.x -= m_CameraMoveSpeed * ts;
		else if (PlagC::Input::IsKeyPressed(PC_KEY_RIGHT))
			m_CameraPosition.x += m_CameraMoveSpeed * ts;

		if (PlagC::Input::IsKeyPressed(PC_KEY_UP))
			m_CameraPosition.y += m_CameraMoveSpeed * ts;
		else if (PlagC::Input::IsKeyPressed(PC_KEY_DOWN))
			m_CameraPosition.y -= m_CameraMoveSpeed * ts;

		if (PlagC::Input::IsKeyPressed(PC_KEY_A))
			m_CameraRotation += m_CameraRotationSpeed * ts;
		if (PlagC::Input::IsKeyPressed(PC_KEY_D))
			m_CameraRotation -= m_CameraRotationSpeed * ts;

		PlagC::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
		PlagC::RenderCommand::Clear();

		m_Camera.SetPosition(m_CameraPosition);
		m_Camera.SetRotation(m_CameraRotation);
		//END

		PlagC::Renderer::BeginScene(m_Camera);

		//TRANSFORMATION TESTING
		//BEGIN

		for(int i = 0; i < 20; ++i)
		{
			for (int j = 0; j < 20; ++j)
			{
				glm::mat4 transform = glm::translate(glm::mat4(1.f), glm::vec3(i * 0.11f, j * 0.11f, 0.f)) *
					glm::mat4(glm::scale(glm::mat4(1.f), glm::vec3(0.1f)));
				PlagC::Renderer::Submit(m_ShaderLibrary.Get("color"), m_SquareVA, transform);
			}
		}

		m_Texture->Bind();
		PlagC::Renderer::Submit(m_ShaderLibrary.Get("texture"), m_SquareVA, glm::mat4(1.f));
		m_LogoTexture->Bind();
		PlagC::Renderer::Submit(m_ShaderLibrary.Get("texture"), m_SquareVA, glm::mat4(1.f));

		//END
		////TRANSFORMATION TESTING
		PlagC::Renderer::EndScene();

	}
	void OnEvent(PlagC::Event& e) override
	{

	}

private:

	PlagC::ShaderLibrary m_ShaderLibrary;

	PlagC::Ref<PlagC::VertexArray> m_VertexArray;
	PlagC::Ref<PlagC::VertexArray> m_SquareVA;

	PlagC::Ref<PlagC::VertexBuffer> m_VertexBuffer;
	PlagC::Ref<PlagC::IndexBuffer> m_IndexBuffer;

	PlagC::Ref<PlagC::Texture2D> m_Texture;
	PlagC::Ref<PlagC::Texture2D> m_LogoTexture;

	glm::vec3 m_SquareColor = { 0.2f, 0.3f, 0.8f };

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