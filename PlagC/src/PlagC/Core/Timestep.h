#pragma once

namespace PlagC
{


	class Timestep
	{
	public:
		Timestep(float time = 0.f) :
			m_Time(time)
		{

		}

		operator float() const { return m_Time; }
		const float GetSeconds() { return m_Time; }
		const float GetMiliSeconds() { return m_Time * 1000.f; }

	private:

		float m_Time;
	};
}