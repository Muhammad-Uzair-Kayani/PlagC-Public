#pragma once
#include "Event.h"

namespace PlagC
{
	class PC_API WindowResizeEvent : public Event
	{

	public:

		WindowResizeEvent(unsigned int Width, unsigned int Height)
			: m_Width(Width), m_Height(Height) {
		}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << GetName() << ": " << m_Width << ", " << m_Height;
			return ss.str();
		}

		inline unsigned int GetWidth() const { return m_Width; }
		inline unsigned int GetHeight() const { return m_Height; }

		EVENT_CLASS_TYPE(WindowResize);
		EVENT_CLASS_CATEGORY(EventCategoryApplication);

	private:
		unsigned int m_Width, m_Height;
	};

	class PC_API WindowCloseEvent : public Event
	{

	public:

		WindowCloseEvent() { }

		EVENT_CLASS_TYPE(WindowClose);
		EVENT_CLASS_CATEGORY(EventCategoryApplication);

	private:

	};

	class PC_API AppTickEvent : public Event
	{

	public:

		AppTickEvent() {}

		EVENT_CLASS_TYPE(ApplicationTick);
		EVENT_CLASS_CATEGORY(EventCategoryApplication);

	private:

	};

	class PC_API AppUpdateEvent : public Event
	{

	public:

		AppUpdateEvent() {}

		EVENT_CLASS_TYPE(ApplicationUpdate);
		EVENT_CLASS_CATEGORY(EventCategoryApplication);

	private:

	};

	class PC_API AppRenderEvent : public Event
	{

	public:

		AppRenderEvent() {}

		EVENT_CLASS_TYPE(ApplicationRender);
		EVENT_CLASS_CATEGORY(EventCategoryApplication);

	private:

	};

}