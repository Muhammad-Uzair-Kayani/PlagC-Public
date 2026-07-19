#pragma once
#include "PlagC/Input.h"

namespace PlagC
{

	class WindowsInput :
		public Input
	{

	public:



	protected:

		virtual bool IsKeyPressedImpl(int keycode) override;
		virtual bool IsMouseButtonPressedImpl(int button) override;
		virtual std::pair<float, float> GetMousePositionImpl() override;
		virtual float GetMouseXImpl() override;
		virtual float GetMouseYImpl() override;

	};

}


