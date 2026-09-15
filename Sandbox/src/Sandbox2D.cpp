#include "Sandbox2D.h"
#include <imgui.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

Sandbox2D::Sandbox2D():
	Layer("Sandbox2D Layer"), m_CameraController(1280.f / 720.f, true)
{

}

void Sandbox2D::OnAttach()
{
	PC_TRACE("Sandbox2D Attached");
	m_CheckerBoardTexture = PlagC::Texture2D::Create("assets/textures/Checkerboard.png");
}

void Sandbox2D::OnDetach()
{

}

void Sandbox2D::OnUpdate(PlagC::Timestep ts)
{
	// Update
	m_CameraController.OnUpdate(ts);

	// Render
	PlagC::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1 });
	PlagC::RenderCommand::Clear();

	PlagC::Renderer2D::BeginScene(m_CameraController.GetCamera());
	PlagC::Renderer2D::DrawQuad({ -1.0f, 0.0f }, { 0.8f, 0.8f }, { 0.8f, 0.2f, 0.3f, 1.0f });
	PlagC::Renderer2D::DrawQuad({ 0.5f, -0.5f }, { 0.5f, 0.75f }, { 0.2f, 0.3f, 0.8f, 1.0f });
	PlagC::Renderer2D::DrawQuad({ 0.0f, 0.0f, -0.1f }, { 10.0f, 10.0f }, m_CheckerBoardTexture);
	PlagC::Renderer2D::EndScene();
}

void Sandbox2D::OnEvent(PlagC::Event& e)
{
	m_CameraController.OnEvent(e);
}

void Sandbox2D::OnImGuiRender()
{

}
