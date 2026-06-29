#include "Main_Window.h"
#include "backend.h"
#include "Widgets_Controls.h"
#include "glad/glad.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <string>
#include <iostream>
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>

using namespace Widgets_Controls;
using namespace mavsdk;

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

        ImGui::Begin("Main",nullptr, window_flags);
        window_size = ImGui::GetWindowSize();
        ImGui::PopStyleVar(2);

        // Set up the dockspace
        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }
        dockspace_flags |= ImGuiDockNodeFlags_AutoHideTabBar;

        ImGui::End();
    }

    void Mycode() {
        embraceTheDarkness();


		static bool ROLL_window_bool = false;
		static bool Air_Speed_window_bool = false;
		static bool Vertical_Speed_window_bool = false;
		static bool altitude_window_bool = false;
		static bool PITCH_window_bool = false;
		static bool Compass_window_bool = false;
		static bool BATTERY_window_bool = false;
		static bool LOCATION_window_bool = true;
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
            ImGui::EndMainMenuBar();
        }

       


       

        ImGui::Begin("print");

        ImGui::Text("battery_temperature_deg         =  %f ",  backend::Battery_temperature_deg.load());
        ImGui::Text("battery_voltage                 =  %f ",  backend::Battery_voltage.load());
        ImGui::Text("battery_current_battery         =  %f ",  backend::Battery_current_battery.load());
        ImGui::Text("battery_capacity_consumed       =  %f ",  backend::Battery_capacity_consumed.load());
        ImGui::Text("battery_remaining_percent       =  %f ",  backend::Battery_remaining_percent.load());
        ImGui::Text("battery_time_remaining          =  %f ",  backend::Battery_time_remaining.load());
        ImGui::Text("position_altitude               =  %f ",  backend::Position_altitude.load());
        ImGui::Text("position_latitude               =  %f ",  backend::Position_latitude.load());
        ImGui::Text("position_longitude              =  %f ",  backend::Position_longitude.load());
        ImGui::Text("position_relative_altitude      =  %f ",  backend::Position_relative_altitude.load());
        ImGui::Text("angular_velocity_roll_rad       =  %f ",  backend::Angular_velocity_roll_rad.load());
        ImGui::Text("angular_velocity_pitch_rad      =  %f ",  backend::Angular_velocity_pitch_rad.load());
        ImGui::Text("angular_velocity_yaw_rad        =  %f ",  backend::Angular_velocity_yaw_rad.load());
        ImGui::Text("gps_fix_type                    =  %i ",  backend::GPS_fix_type.load());
        ImGui::Text("is_gyrometer_calibration_ok     =  %i ",  backend::Is_gyrometer_calibration_ok.load());
        ImGui::Text("is_accelerometer_calibration_ok =  %i ",  backend::Is_accelerometer_calibration_ok.load());
        ImGui::Text("is_magnetometer_calibration_ok  =  %i ",  backend::Is_magnetometer_calibration_ok.load());
        ImGui::Text("is_local_position_ok            =  %i ",  backend::Is_local_position_ok .load());
        ImGui::Text("is_global_position_ok           =  %i ",  backend::Is_global_position_ok.load());
        ImGui::Text("is_home_position_ok             =  %i ",  backend::Is_home_position_ok.load());
        ImGui::Text("is_armable                      =  %i ",  backend::Is_armable.load());
        ImGui::Text("is_armed                        =  %i ",  backend::Is_armed.load());
        ImGui::Text("velocity_x_m_s                  =  %f ",  backend::Velocity_x_m_s.load());
        ImGui::Text("velocity_y_m_s                  =  %f ",  backend::Velocity_y_m_s.load());
        ImGui::Text("velocity_z_m_s                  =  %f ",  backend::Velocity_z_m_s.load());
        ImGui::Text("was_available_once              =  %i ",  backend::Was_available_once.load());
        ImGui::Text("is_available                    =  %i ",  backend::Is_available.load());
        ImGui::Text("signal_strength_percent         =  %i ",  backend::Signal_strength_percent.load());

        ImGui::End();



    }


}


