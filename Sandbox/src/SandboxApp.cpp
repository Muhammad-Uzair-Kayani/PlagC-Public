#include <PlagC.h>
//---------------------------------
//Entry Point
#include "PlagC/Core/StartingPoint.h"
//---------------------------------
#include "imgui.h"
#include "Sandbox2D.h"

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
		
	}

	void OnUpdate(PlagC::Timestep ts) override
	{
	
	}
	void OnEvent(PlagC::Event& e) override
	{
		
	}

private:

	

};

class Sandbox : public PlagC::Application
{
public:
	Sandbox()
	{
		// Initialize your application here
		PushLayer(new ApplcationLayer());
		PushLayer(new Sandbox2D());

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