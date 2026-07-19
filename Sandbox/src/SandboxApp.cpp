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
		PushOverLay(new PlagC::ImGuiLayer());
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