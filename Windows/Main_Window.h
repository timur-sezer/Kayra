#pragma once
#include "Widgets_Controls.h"

namespace MyWindow {
	bool Init();          // load textures; call once after the GL context and ImGui are ready
	void Shutdown();      // free textures
	void ShowMainDockSpace();
	void Mycode();
}
