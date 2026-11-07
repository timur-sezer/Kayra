#pragma once
#include "imgui.h"

namespace AttitudeIndicator {
	// Window title, used by the dock layout.
	inline constexpr const char* kWindowTitle = "Attitude";

	// Draws the attitude indicator at the current cursor position, filling a square of side `size`.
	// pitch_deg: nose up positive.  roll_deg: right wing down positive.
	void Draw(float pitch_deg, float roll_deg, float size);

	// Shows the "Attitude" window with the indicator fitted to the window.
	// Until live telemetry is connected, right-click the window to set pitch/roll by hand
	// or turn on demo motion.
	void ShowWindow(bool* p_open);

	// Feed live attitude here (e.g. from MAVSDK telemetry). Turns off demo/manual input.
	void SetAttitude(float pitch_deg, float roll_deg);
}
