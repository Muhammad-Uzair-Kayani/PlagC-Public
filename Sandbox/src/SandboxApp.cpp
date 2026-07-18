#include <PlagC.h>

class ApplcationLayer : public PlagC::Layer
{

public:
	ApplcationLayer()
	{
		PC_INFO("APPLICATION LAYER CREATED");
	}
	~ApplcationLayer() {}

	void OnUpdate() override
	{
		PC_INFO("ExampleLayer::Update");
	}

	void OnEvent(PlagC::Event& event) override
	{
		PC_TRACE("{0}", event.ToString());
	}
};

class Sandbox : public PlagC::Application
{
public:
	Sandbox()
	{
		// Initialize your application here
		PushLayer(new ApplcationLayer());
		PushLayer(new PlagC::ImGuiLayer());
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