#include "Main_Window.h"
#include "Widgets_Controls.h"
#include "glad/glad.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <string>


using namespace Widgets_Controls;

namespace MyWindow {
    ImVec2 window_size;
    void ShowMainDockSpace()
    {
        static bool opt_fullscreen_persistant = true;
        bool opt_fullscreen = opt_fullscreen_persistant;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiDockNodeFlags_AutoHideTabBar;
        if (opt_fullscreen)
        {
            ImGuiViewport* viewport = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(viewport->Pos);
            ImGui::SetNextWindowSize(viewport->Size);
            ImGui::SetNextWindowViewport(viewport->ID);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiDockNodeFlags_AutoHideTabBar;
            window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
        }

        // Important: We need a background window to act as the DockSpace host
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::Begin("Main", nullptr, window_flags);
        window_size = ImGui::GetWindowSize();
        ImGui::PopStyleVar(2);

        // Set up the dockspace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }
        dockspace_flags ^= ImGuiDockNodeFlags_AutoHideTabBar;

        ImGui::End();
    }

    void Mycode() {
        embraceTheDarkness();

        GLuint airspeed_bg;
        GLuint airspeed_hand;
        GLuint verticalspeed_bg;
        GLuint verticalspeed_hand;
        GLuint altitude_bg;
        GLuint compass_bg;
        GLuint compass_handle;
        GLuint roll_bg;
        GLuint roll_handle;
        GLuint handle;
        GLuint settings_icon;

        int my_image_width;
        int my_image_height;


        bool ret  = LoadTextureFromFile("../assets/images/ais/ais_case_v10.png", &airspeed_bg, &my_image_width, &my_image_height);
        bool ret0 = LoadTextureFromFile("../assets/images/ais/ais_hand_v3.png", &airspeed_hand, &my_image_width, &my_image_height);
        bool ret1 = LoadTextureFromFile("../assets/images/vsi/vsi_case.png", &verticalspeed_bg, &my_image_width, &my_image_height);
        bool ret2 = LoadTextureFromFile("../assets/images/vsi/vsi_hand.png", &verticalspeed_hand, &my_image_width, &my_image_height);
        bool ret3 = LoadTextureFromFile("../assets/images/alt/alt_case_v2.png", &altitude_bg, &my_image_width, &my_image_height);
        bool ret4 = LoadTextureFromFile("../assets/images/hsi/hsi_v2.png", &compass_bg, &my_image_width, &my_image_height);
        bool ret5 = LoadTextureFromFile("../assets/images/hsi/hsi_face_v2.png", &compass_handle, &my_image_width, &my_image_height);
        bool ret6 = LoadTextureFromFile("../assets/images/tc/tc_case_v2.png", &roll_bg, &my_image_width, &my_image_height);
        bool ret7 = LoadTextureFromFile("../assets/images/tc/tc_drone.png", &roll_handle, &my_image_width, &my_image_height);
        bool ret8 = LoadTextureFromFile("../assets/images/tc/tc_drone.png", &handle, &my_image_width, &my_image_height);
        IM_ASSERT(ret && ret0 && ret1 && ret2 && ret3 && ret4 && ret5 && ret6 && ret7 && ret8);


		static bool ROLL_window_bool = true;
		static bool Air_Speed_window_bool = true;
		static bool Vertical_Speed_window_bool = true;
		static bool altitude_window_bool = true;
		static bool PITCH_window_bool = true;
		static bool Compass_window_bool = true;
		static bool BATTERY_window_bool = false;
		static bool LOCATION_window_bool = false;
		static bool DATA_window_bool = false;


		ShowMainDockSpace();

        
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("OPEN-WINDOW"))
            {
                if (ImGui::MenuItem("Vertical Speed")) {
                    Vertical_Speed_window_bool = !Vertical_Speed_window_bool;
                }
                if (ImGui::MenuItem("Air Speed")) {
                    Air_Speed_window_bool = !Air_Speed_window_bool;
                }
                if (ImGui::MenuItem("Altitude")) {
                    altitude_window_bool = !altitude_window_bool;
                }
                if (ImGui::MenuItem("Compass")) {
                    Compass_window_bool = !Compass_window_bool;
                }
                if (ImGui::MenuItem("BATTERY-SETTINGS")) {
                    BATTERY_window_bool = !BATTERY_window_bool;
                }
                if (ImGui::MenuItem("Roll")) {
                    ROLL_window_bool = !ROLL_window_bool;
                }
                if (ImGui::MenuItem("LOCATION")) {
                    LOCATION_window_bool = !LOCATION_window_bool;
                }
                ImGui::EndMenu();
            }
            

            ImGui::SetCursorPos(ImVec2(window_size.x-120 ,3));
			ImGui::Button("Settings", ImVec2(100,25));
            ImGui::EndMainMenuBar();
        }

        if (Air_Speed_window_bool) {
            ImGui::Begin("Air_Speed", &Air_Speed_window_bool);
            ImVec2 Air_Speed_window_size = ImGui::GetWindowSize();

            ImGui::SetCursorPos(ImVec2(5, 50));
            const ImVec2 Air_Speed_window_child = ImVec2(Air_Speed_window_size.x - 10, Air_Speed_window_size.y - 55);
            ImGui::BeginChild("Air_Speed_WINDOW_CHILD", Air_Speed_window_child);

            ImVec2 Air_Speed_gauge_value = ImGui::GetCursorScreenPos();
            if (Air_Speed_window_size.x <= Air_Speed_window_size.y) {
                float image_position = (Air_Speed_window_child.y - Air_Speed_window_child.x + 6) / 2 + 20;
                ImGui::SetCursorPos(ImVec2(30, image_position + 7));
                ImGui::Image((void*)(intptr_t)airspeed_bg, ImVec2(Air_Speed_window_child.x - 60, Air_Speed_window_child.x - 60));


                ImVec2 center(Air_Speed_gauge_value.x + Air_Speed_window_child.x / 2, Air_Speed_gauge_value.y + Air_Speed_window_child.y / 2);
                ImVec2 size(Air_Speed_window_child.x - 60, Air_Speed_window_child.x - 60);
                Widgets_Controls::ImageRotated((void*)(intptr_t)airspeed_hand, center, size, 0.01 * 6.3);

            }
            else {
                float image_position = (Air_Speed_window_child.x - Air_Speed_window_child.y + 6) / 2 + 5;
                ImGui::SetCursorPos(ImVec2(image_position, 8));
                ImGui::Image((void*)(intptr_t)airspeed_bg, ImVec2(Air_Speed_window_child.y - 15, Air_Speed_window_child.y - 15));


                ImVec2 center(Air_Speed_gauge_value.x + Air_Speed_window_child.x / 2, Air_Speed_gauge_value.y + Air_Speed_window_child.y / 2);
                ImVec2 size(Air_Speed_window_child.y - 15, Air_Speed_window_child.y - 15);
                Widgets_Controls::ImageRotated((void*)(intptr_t)airspeed_hand, center, size, 0.01 * 6.3);
            }
            ImGui::EndChild();


            ImGui::End();

        }



        if (Vertical_Speed_window_bool) {
            ImGui::Begin("Vertical_Speed", &Vertical_Speed_window_bool);
            ImVec2 Vertical_Speed_window_size = ImGui::GetWindowSize();

            ImGui::SetCursorPos(ImVec2(5, 50));
            const ImVec2 Vertical_Speed_window_child = ImVec2(Vertical_Speed_window_size.x - 10, Vertical_Speed_window_size.y - 55);
            ImGui::BeginChild("Vertical_Speed_WINDOW_CHILD", Vertical_Speed_window_child);

            ImVec2 Vertical_Speed_gauge_value = ImGui::GetCursorScreenPos();
            if (Vertical_Speed_window_size.x <= Vertical_Speed_window_size.y) {
                float image_position = (Vertical_Speed_window_child.y - Vertical_Speed_window_child.x + 6) / 2 + 20;
                ImGui::SetCursorPos(ImVec2(30, image_position + 7));
                ImGui::Image((void*)(intptr_t)verticalspeed_bg, ImVec2(Vertical_Speed_window_child.x - 60, Vertical_Speed_window_child.x - 60));


                ImVec2 center(Vertical_Speed_gauge_value.x + Vertical_Speed_window_child.x / 2, Vertical_Speed_gauge_value.y + Vertical_Speed_window_child.y / 2);
                ImVec2 size(Vertical_Speed_window_child.x - 60, Vertical_Speed_window_child.x - 60);
                Widgets_Controls::ImageRotated((void*)(intptr_t)verticalspeed_hand, center, size, 0.01 * 6.3);

            }
            else {
                float image_position = (Vertical_Speed_window_child.x - Vertical_Speed_window_child.y + 6) / 2 + 5;
                ImGui::SetCursorPos(ImVec2(image_position, 8));
                ImGui::Image((void*)(intptr_t)verticalspeed_bg, ImVec2(Vertical_Speed_window_child.y - 15, Vertical_Speed_window_child.y - 15));


                ImVec2 center(Vertical_Speed_gauge_value.x + Vertical_Speed_window_child.x / 2, Vertical_Speed_gauge_value.y + Vertical_Speed_window_child.y / 2);
                ImVec2 size(Vertical_Speed_window_child.y - 15, Vertical_Speed_window_child.y - 15);
                Widgets_Controls::ImageRotated((void*)(intptr_t)verticalspeed_hand, center, size, 0.01 * 6.3);
            }
            ImGui::EndChild();


            ImGui::End();

        }



        if (altitude_window_bool) {
            ImGui::Begin("altitude", &altitude_window_bool);
            ImVec2 altitude_window_size = ImGui::GetWindowSize();

            ImGui::SetCursorPos(ImVec2(5, 50));
            const ImVec2 altitude_window_child = ImVec2(altitude_window_size.x - 10, altitude_window_size.y - 55);
            ImGui::BeginChild("altitude_WINDOW_CHILD", altitude_window_child);

            ImVec2 altitude_gauge_value = ImGui::GetCursorScreenPos();
            if (altitude_window_size.x <= altitude_window_size.y) {
                float image_position = (altitude_window_child.y - altitude_window_child.x + 6) / 2 + 20;
                ImGui::SetCursorPos(ImVec2(30, image_position + 7));
                ImGui::Image((void*)(intptr_t)altitude_bg, ImVec2(altitude_window_child.x - 60, altitude_window_child.x - 60));


                ImVec2 center(altitude_gauge_value.x + altitude_window_child.x / 2, altitude_gauge_value.y + altitude_window_child.y / 2);
                ImVec2 size(altitude_window_child.x - 60, altitude_window_child.x - 60);
                Widgets_Controls::ImageRotated((void*)(intptr_t)airspeed_hand, center, size, 0.01 * 6.3);

            }
            else {
                float image_position = (altitude_window_child.x - altitude_window_child.y + 6) / 2 + 5;
                ImGui::SetCursorPos(ImVec2(image_position, 8));
                ImGui::Image((void*)(intptr_t)altitude_bg, ImVec2(altitude_window_child.y - 15, altitude_window_child.y - 15));


                ImVec2 center(altitude_gauge_value.x + altitude_window_child.x / 2, altitude_gauge_value.y + altitude_window_child.y / 2);
                ImVec2 size(altitude_window_child.y - 15, altitude_window_child.y - 15);
                Widgets_Controls::ImageRotated((void*)(intptr_t)airspeed_hand, center, size, 0.01 * 6.3);
            }
            ImGui::EndChild();


            ImGui::End();

        }



        if (Compass_window_bool) {
            ImGui::Begin("Compass", &Compass_window_bool);
            ImVec2 Compass_window_size = ImGui::GetWindowSize();

            ImGui::SetCursorPos(ImVec2(5, 50));
            const ImVec2 Compass_window_child = ImVec2(Compass_window_size.x - 10, Compass_window_size.y - 55);
            ImGui::BeginChild("Compass_WINDOW_CHILD", Compass_window_child);

            ImVec2 Compass_gauge_value = ImGui::GetCursorScreenPos();
            if (Compass_window_size.x <= Compass_window_size.y) {
                float image_position = (Compass_window_child.y - Compass_window_child.x + 6) / 2 + 20;
                ImGui::SetCursorPos(ImVec2(30, image_position + 7));
                ImGui::Image((void*)(intptr_t)compass_bg, ImVec2(Compass_window_child.x - 60, Compass_window_child.x - 60));


                ImVec2 center(Compass_gauge_value.x + Compass_window_child.x / 2, Compass_gauge_value.y + Compass_window_child.y / 2);
                ImVec2 size(Compass_window_child.x - 60, Compass_window_child.x - 60);
                Widgets_Controls::ImageRotated((void*)(intptr_t)compass_handle, center, size, 0.01 * 6.3);

            }
            else {
                float image_position = (Compass_window_child.x - Compass_window_child.y + 6) / 2 + 5;
                ImGui::SetCursorPos(ImVec2(image_position, 8));
                ImGui::Image((void*)(intptr_t)compass_bg, ImVec2(Compass_window_child.y - 15, Compass_window_child.y - 15));


                ImVec2 center(Compass_gauge_value.x + Compass_window_child.x / 2, Compass_gauge_value.y + Compass_window_child.y / 2);
                ImVec2 size(Compass_window_child.y - 15, Compass_window_child.y - 15);
                Widgets_Controls::ImageRotated((void*)(intptr_t)compass_handle, center, size, 0.01 * 6.3);
            }
            ImGui::EndChild();


            ImGui::End();

        }

        
        
        if (ROLL_window_bool) {
            ImGui::Begin("ROLL", &ROLL_window_bool);
            ImVec2 ROLL_window_size = ImGui::GetWindowSize();

            ImGui::SetCursorPos(ImVec2(5, 50));
            const ImVec2 ROLL_window_child = ImVec2(ROLL_window_size.x - 10, ROLL_window_size.y - 55);
            ImGui::BeginChild("ROLL_WINDOW_CHILD", ROLL_window_child);

            ImVec2 ROLL_gauge_value = ImGui::GetCursorScreenPos();
            if (ROLL_window_size.x <= ROLL_window_size.y) {
                float image_position = (ROLL_window_child.y - ROLL_window_child.x + 6) / 2 + 20;
                ImGui::SetCursorPos(ImVec2(30, image_position + 7));
                ImGui::Image((void*)(intptr_t)roll_bg, ImVec2(ROLL_window_child.x - 60, ROLL_window_child.x - 60));


                ImVec2 center(ROLL_gauge_value.x + ROLL_window_child.x / 2, ROLL_gauge_value.y + ROLL_window_child.y / 2);
                ImVec2 size(ROLL_window_child.x - 60, ROLL_window_child.x - 60);
                Widgets_Controls::ImageRotated((void*)(intptr_t)roll_handle, center, size,0.7);

            }
            else {
                float image_position = (ROLL_window_child.x - ROLL_window_child.y + 6) / 2 + 5;
                ImGui::SetCursorPos(ImVec2(image_position, 8));
                ImGui::Image((void*)(intptr_t)roll_bg, ImVec2(ROLL_window_child.y - 15, ROLL_window_child.y - 15));


                ImVec2 center(ROLL_gauge_value.x + ROLL_window_child.x / 2, ROLL_gauge_value.y + ROLL_window_child.y / 2);
                ImVec2 size(ROLL_window_child.y - 15, ROLL_window_child.y - 15);
                Widgets_Controls::ImageRotated((void*)(intptr_t)roll_handle, center, size,0.7);
            }
            ImGui::EndChild();


            ImGui::End();

        }

        
    }


}