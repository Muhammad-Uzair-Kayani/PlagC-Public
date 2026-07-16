#include <PlagC.h>

class Sandbox : public PlagC::Application
{
public:
	Sandbox()
	{
		// Initialize your application here
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