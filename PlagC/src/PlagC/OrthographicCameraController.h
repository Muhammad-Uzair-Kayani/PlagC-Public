#pragma once

#include "PlagC/Renderer/OrthographicCamera.h"
#include "PlagC/Core/Timestep.h"

#include "PlagC/Events/ApplicationEvent.h"
#include "PlagC/Events/MouseEvent.h"

namespace PlagC
{
	class OrthographicCameraController
	{
	public:

		OrthographicCameraController(const float aspectRatio, const bool rotation = false);

		void OnUpdate(const Timestep ts);
		void OnEvent(Event& e);

		OrthographicCamera& GetCamera() { return m_Camera; }
		const OrthographicCamera& GetCamera() const { return m_Camera; }

	private:

		bool OnMouseScrolled(MouseScrolledEvent& e);
		bool OnWindowResized(WindowResizeEvent& e);

	private:
		float m_AspectRatio;
		bool m_Rotation;
		float m_ZoomLevel = 1.0f;
		OrthographicCamera m_Camera;

		glm::vec3 m_CameraPosition = { 0.0f, 0.0f, 0.0f };
		float m_CameraRotation = 0.0f;
		float m_CameraTranslationSpeed = 0.5f, m_CameraRotationSpeed = 10.f;
	};
}


