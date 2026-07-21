#include <PlagC.h>
#include "imgui.h"

class ApplcationLayer : public PlagC::Layer
{

public:
	ApplcationLayer()
	{
		PC_INFO("APPLICATION LAYER CREATED");
	}
	~ApplcationLayer() {}

	void OnImGuiRender()
	{
		ImGui::Begin("Test");
		ImGui::Text("Hello World");
		ImGui::End();
	}

	void OnUpdate() override
	{
	
	}
	void OnEvent(PlagC::Event& e) override
	{

	}
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