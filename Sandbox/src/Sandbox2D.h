#pragma once

#include <PlagC.h>

class Sandbox2D : public PlagC::Layer
{
public:

	Sandbox2D();
	virtual ~Sandbox2D() = default;

	void OnAttach() override;
	void OnDetach() override;

	void OnUpdate(PlagC::Timestep ts) override;
	void OnEvent(PlagC::Event& e) override;
	void OnImGuiRender() override;

private:

	PlagC::OrthographicCameraController m_CameraController;

	PlagC::Ref<PlagC::Texture> m_CheckerBoardTexture;
};

