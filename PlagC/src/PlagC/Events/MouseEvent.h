#pragma once
#include "Event.h"

namespace PlagC
{
	class PC_API MouseMovedEvent : public Event
	{
	public:

		MouseMovedEvent(float x, float y)
			: m_MouseX(x), m_MouseY(y) {
		}

		inline float GetX() const { return m_MouseX; }
		inline float GetY() const { return m_MouseY; }

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseMovedEvent: " << m_MouseX << ", " << m_MouseY;
			return ss.str();
		}

		EVENT_CLASS_TYPE(MouseMoved);
		EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

	private:
		float m_MouseX, m_MouseY;
	};

	class PC_API MouseScrolledEvent : public Event
	{
	public:

		MouseScrolledEvent(float OffsetX, float OffsetY) : m_XOffset(OffsetX), m_YOffset(OffsetY) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseScrolledEvent: " << m_XOffset << ", " << m_YOffset;
			return ss.str();
		}

		inline float GetXOffset() const { return m_XOffset; }
		inline float GetYOffset() const { return m_YOffset; }

		EVENT_CLASS_TYPE(MouseScrolled);
		EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

	private:
		float m_XOffset, m_YOffset;
	};

	class PC_API MouseButtonEvent : public Event
	{
	public:

		MouseButtonEvent(int button) : m_MouseButton(button) { }

		inline float GetMouseButton() const { return m_MouseButton; }

		EVENT_CLASS_CATEGORY(EventCategoryMouse | EventCategoryInput)

		protected:
			int m_MouseButton;
	};

	class PC_API MouseButtonPressedEvent : public MouseButtonEvent
	{
	public:

		MouseButtonPressedEvent(int button) : MouseButtonEvent(button) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseButtonPressedEvent: " << m_MouseButton;
			return ss.str();
		}

		EVENT_CLASS_TYPE(MouseButtonPressed)
		
	};

	class PC_API MouseButtonReleasedEvent : public MouseButtonEvent
	{
	public:

		MouseButtonReleasedEvent(int button) : MouseButtonEvent(button) {}

		std::string ToString() const override
		{
			std::stringstream ss;
			ss << "MouseButtonReleasedEvent: " << m_MouseButton;
			return ss.str();
		}

		EVENT_CLASS_TYPE(MouseButtonReleased)

	};
}