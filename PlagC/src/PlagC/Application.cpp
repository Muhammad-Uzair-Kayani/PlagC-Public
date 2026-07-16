#include "Application.h"
#include "log.h"

#define PC_BIND_EVENT_FN(fn) std::bind(&fn, this, std::placeholders::_1)

PlagC::Application::Application()
{

}

PlagC::Application::~Application()
{

}

void PlagC::Application::Run()
{

	WindowResizeEvent e(1280, 720);

	if (e.GetEventType() == EventType::WindowResize)
	{
		PC_TRACE(e.ToString());
	}
	if (e.GetEventType() == EventType::ApplicationRender)
	{
		PC_TRACE(e.ToString());
	}

	EventDispatcher dispatcher(e);

	dispatcher.Dispatch<WindowResizeEvent>(PC_BIND_EVENT_FN(PlagC::Application::OnWindowClose));

	while (true)
	{
	}
}

bool PlagC::Application::OnWindowClose(WindowResizeEvent& e)
{
	PC_TRACE(e.ToString() + " Event Dispatched");
	return true;
}

