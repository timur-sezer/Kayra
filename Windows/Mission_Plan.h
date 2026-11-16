#pragma once

// "Mission Plan" tab: three separate dockable windows that edit the same plan.
//   Map:         world map with the live vehicle, trail, home and mission. Click to add waypoints,
//                drag them to move them.
//   Node editor: the mission as a node graph in flight order.
//   Mission:     upload/download, item list, item editor and Export JSON.
// The plan is uploaded to / downloaded from the vehicle through the MAVSDK backend.

#include <string>

namespace MissionPlan {
    void Update();      // call every frame, whichever tab is open (keeps vehicle trail and mission in sync)
    // Window titles (the part after ### is the stable ImGui id)
    inline constexpr const char* kMapTitle   = "Map###PlanMap";
    inline constexpr const char* kGraphTitle = "Node editor###PlanGraph";
    inline constexpr const char* kPanelTitle = "Mission###PlanPanel";

    void BuildDefaultLayout(unsigned dockspace_id, float width, float height);  // DockBuilder layout of the tab
    bool LayoutBuilt();
    void ResetLayout();     // rebuild the default layout on the next frame
    void ShowWindows();     // submit the three windows (only while the Mission Plan tab is shown)
    void WindowsMenu();     // OPEN-WINDOW menu entries for this tab
    void Shutdown();    // frees map textures and stops tile downloads; call before destroying the GL context

    std::string TileCacheDir();
    void ClearTileCache();
}
