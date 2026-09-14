#include "pch.h"
#include "OrthographicCameraController.h"

#include "PlagC/Core/Input.h"
#include "PlagC/Core/KeyCodes.h"

PlagC::OrthographicCameraController::OrthographicCameraController(const float aspectRatio, const bool rotation):
	m_AspectRatio(aspectRatio), m_Camera(-m_AspectRatio * m_ZoomLevel, m_AspectRatio * m_ZoomLevel,
		-m_ZoomLevel, m_ZoomLevel), m_Rotation(rotation)
{

}

void PlagC::OrthographicCameraController::OnUpdate(const Timestep ts)
{
	if (Input::IsKeyPressed(PC_KEY_A))
		m_CameraPosition.x -= m_CameraTranslationSpeed * ts;
	else if (Input::IsKeyPressed(PC_KEY_D))
		m_CameraPosition.x += m_CameraTranslationSpeed * ts;

	if (Input::IsKeyPressed(PC_KEY_W))
		m_CameraPosition.y += m_CameraTranslationSpeed * ts;
	else if (Input::IsKeyPressed(PC_KEY_S))
		m_CameraPosition.y -= m_CameraTranslationSpeed * ts;

	if (m_Rotation)
	{
		if (Input::IsKeyPressed(PC_KEY_Q))
			m_CameraRotation += m_CameraRotationSpeed * ts;
		if (Input::IsKeyPressed(PC_KEY_E))
			m_CameraRotation -= m_CameraRotationSpeed * ts;

		m_Camera.SetRotation(m_CameraRotation);
	}

	m_Camera.SetPosition(m_CameraPosition);

	m_CameraTranslationSpeed = m_ZoomLevel;
}

void PlagC::OrthographicCameraController::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<MouseScrolledEvent>(PC_BIND_EVENT_FN(PlagC::OrthographicCameraController::OnMouseScrolled));
	dispatcher.Dispatch<WindowResizeEvent>(PC_BIND_EVENT_FN(PlagC::OrthographicCameraController::OnWindowResized));
}

bool PlagC::OrthographicCameraController::OnMouseScrolled(MouseScrolledEvent& e)
{
	m_ZoomLevel -= e.GetYOffset();
	m_ZoomLevel = std::max(m_ZoomLevel, 0.25f);

	m_Camera.SetProjection(-m_AspectRatio * m_ZoomLevel, 
		m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);

	return false;
}

bool PlagC::OrthographicCameraController::OnWindowResized(WindowResizeEvent& e)
{
	m_AspectRatio = (float)e.GetWidth() / (float)e.GetHeight();
	m_Camera.SetProjection(-m_AspectRatio * m_ZoomLevel, 
		m_AspectRatio * m_ZoomLevel, -m_ZoomLevel, m_ZoomLevel);

	return false;
}
