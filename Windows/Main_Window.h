#pragma once
#include "Widgets_Controls.h"

namespace MyWindow {
	bool Init();          // load textures; call once after the GL context and ImGui are ready
	void Shutdown();      // free textures
	void ShowMainDockSpace(bool flight_visible, bool plan_visible);  // hidden dockspaces keep their layout
	void ResetLayout();                    // rebuild the default Flight layout on the next frame
	void Mycode();
	void SetConnectionUrl(const char* url);  // prefill the Connection window's URL field
}
