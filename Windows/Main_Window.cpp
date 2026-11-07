#include "Main_Window.h"
#include "Widgets_Controls.h"
#include "Attitude_Indicator.h"
#include "imgui.h"
#include "imgui_internal.h"   // DockBuilder API
#include <cstdint>
#include <cstdio>
#include <string>

#ifndef KAYRA_ASSETS_DIR
#define KAYRA_ASSETS_DIR "../assets"
#endif

using namespace Widgets_Controls;

namespace MyWindow {
    static ImVec2 window_size;

    // Textures are created once in Init() — loading them every frame leaks GPU memory.
    static GLuint airspeed_bg = 0;
    static GLuint airspeed_hand = 0;
    static GLuint verticalspeed_bg = 0;
    static GLuint verticalspeed_hand = 0;
    static GLuint altitude_bg = 0;
    static GLuint compass_bg = 0;
    static GLuint compass_handle = 0;
    static GLuint roll_bg = 0;
    static GLuint roll_handle = 0;

    static GLuint* const all_textures[] = {
        &airspeed_bg, &airspeed_hand, &verticalspeed_bg, &verticalspeed_hand,
        &altitude_bg, &compass_bg, &compass_handle, &roll_bg, &roll_handle,
    };

    static ImTextureID ToTexID(GLuint tex) { return (ImTextureID)(intptr_t)tex; }

    static bool LoadAsset(const char* relative_path, GLuint* out_texture)
    {
        std::string path = std::string(KAYRA_ASSETS_DIR) + "/" + relative_path;
        int w = 0, h = 0;
        if (!LoadTextureFromFile(path.c_str(), out_texture, &w, &h)) {
            fprintf(stderr, "KAYRA: failed to load texture '%s'\n", path.c_str());
            return false;
        }
        return true;
    }

    bool Init()
    {
        embraceTheDarkness();

        bool ok = true;
        ok &= LoadAsset("images/ais/ais_case_v10.png", &airspeed_bg);
        ok &= LoadAsset("images/ais/ais_hand_v3.png",  &airspeed_hand);
        ok &= LoadAsset("images/vsi/vsi_case.png",     &verticalspeed_bg);
        ok &= LoadAsset("images/vsi/vsi_hand.png",     &verticalspeed_hand);
        ok &= LoadAsset("images/alt/alt_case_v2.png",  &altitude_bg);
        ok &= LoadAsset("images/hsi/hsi_v2.png",       &compass_bg);
        ok &= LoadAsset("images/hsi/hsi_face_v2.png",  &compass_handle);
        ok &= LoadAsset("images/tc/tc_case_v2.png",    &roll_bg);
        ok &= LoadAsset("images/tc/tc_drone.png",      &roll_handle);
        return ok;
    }

    void Shutdown()
    {
        for (GLuint* tex : all_textures) {
            if (*tex) {
                glDeleteTextures(1, tex);
                *tex = 0;
            }
        }
    }

    // Builds the fixed default layout (standard cockpit "six-pack" order):
    //   +-------------+-------------+-------------+
    //   |  Air_Speed  |  Attitude   |  altitude   |
    //   +-------------+-------------+-------------+
    //   |    ROLL     |   Compass   | Vertical_Sp |
    //   +-------------+-------------+-------------+
    static void BuildDefaultLayout(ImGuiID dockspace_id, ImVec2 size)
    {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, size);

        ImGuiID top = 0, bottom = 0;
        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Up, 0.5f, &top, &bottom);

        ImGuiID top_left = 0, top_rest = 0, top_mid = 0, top_right = 0;
        ImGui::DockBuilderSplitNode(top, ImGuiDir_Left, 1.0f / 3.0f, &top_left, &top_rest);
        ImGui::DockBuilderSplitNode(top_rest, ImGuiDir_Left, 0.5f, &top_mid, &top_right);

        ImGuiID bottom_left = 0, bottom_rest = 0, bottom_mid = 0, bottom_right = 0;
        ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Left, 1.0f / 3.0f, &bottom_left, &bottom_rest);
        ImGui::DockBuilderSplitNode(bottom_rest, ImGuiDir_Left, 0.5f, &bottom_mid, &bottom_right);

        ImGui::DockBuilderDockWindow("Air_Speed",                      top_left);
        ImGui::DockBuilderDockWindow(AttitudeIndicator::kWindowTitle,  top_mid);
        ImGui::DockBuilderDockWindow("altitude",                       top_right);
        ImGui::DockBuilderDockWindow("ROLL",                           bottom_left);
        ImGui::DockBuilderDockWindow("Compass",                        bottom_mid);
        ImGui::DockBuilderDockWindow("Vertical_Speed",                 bottom_right);

        ImGui::DockBuilderFinish(dockspace_id);
    }

    void ShowMainDockSpace()
    {
        const ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        // Note: ImGuiDockNodeFlags_* must not be mixed into ImGuiWindowFlags (different enums).
        ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDocking;
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        // Work area excludes the main menu bar, so the dockspace doesn't sit under it.
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
        window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        // Background window that hosts the DockSpace
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

        ImGui::Begin("Main", nullptr, window_flags);
        window_size = ImGui::GetWindowSize();
        ImGui::PopStyleVar(2);

        ImGuiIO& io = ImGui::GetIO();
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            static bool layout_built = false;
            if (!layout_built) {
                BuildDefaultLayout(dockspace_id, viewport->WorkSize);
                layout_built = true;
            }
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }

        ImGui::End();
    }

    // Draws a square gauge (background + rotated needle) centered in a window.
    static void GaugeWindow(const char* title, bool* p_open, GLuint background, GLuint needle, float angle)
    {
        if (!*p_open)
            return;

        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (ImGui::Begin(title, p_open, flags)) {
            // Use the real free area under the title/tab bar instead of hard-coded offsets.
            const ImVec2 origin = ImGui::GetCursorScreenPos();
            const ImVec2 avail  = ImGui::GetContentRegionAvail();

            // Largest square that fits, with a small margin so the case doesn't touch the edges.
            const float margin = 0.05f;
            const float side   = (avail.x < avail.y ? avail.x : avail.y) * (1.0f - 2.0f * margin);

            if (side > 1.0f) {
                // Case and needle share one center, so the needle always pivots in the dial's middle.
                const ImVec2 center(origin.x + avail.x * 0.5f, origin.y + avail.y * 0.5f);
                const ImVec2 half(side * 0.5f, side * 0.5f);

                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddImage(ToTexID(background),
                             ImVec2(center.x - half.x, center.y - half.y),
                             ImVec2(center.x + half.x, center.y + half.y));
                ImageRotated(ToTexID(needle), center, ImVec2(side, side), angle);
            }

            // Reserve the area so ImGui's layout knows it is used.
            ImGui::Dummy(avail);
        }
        ImGui::End();
    }

    void Mycode() {
        static bool ROLL_window_bool = true;
        static bool Attitude_window_bool = true;
        static bool Air_Speed_window_bool = true;
        static bool Vertical_Speed_window_bool = true;
        static bool altitude_window_bool = true;
        static bool Compass_window_bool = true;
        static bool BATTERY_window_bool = false;
        static bool LOCATION_window_bool = false;

        ShowMainDockSpace();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("OPEN-WINDOW"))
            {
                ImGui::MenuItem("Vertical Speed",   nullptr, &Vertical_Speed_window_bool);
                ImGui::MenuItem("Air Speed",        nullptr, &Air_Speed_window_bool);
                ImGui::MenuItem("Attitude",         nullptr, &Attitude_window_bool);
                ImGui::MenuItem("Altitude",         nullptr, &altitude_window_bool);
                ImGui::MenuItem("Compass",          nullptr, &Compass_window_bool);
                ImGui::MenuItem("BATTERY-SETTINGS", nullptr, &BATTERY_window_bool);
                ImGui::MenuItem("Roll",             nullptr, &ROLL_window_bool);
                ImGui::MenuItem("LOCATION",         nullptr, &LOCATION_window_bool);
                ImGui::EndMenu();
            }

            ImGui::SetCursorPos(ImVec2(window_size.x - 120, 3));
            ImGui::Button("Settings", ImVec2(100, 25));
            ImGui::EndMainMenuBar();
        }

        // Needle angles are placeholders until live telemetry is wired in.
        const float placeholder_angle = 0.01f * 6.3f;
        GaugeWindow("Air_Speed",      &Air_Speed_window_bool,      airspeed_bg,      airspeed_hand,      placeholder_angle);
        GaugeWindow("Vertical_Speed", &Vertical_Speed_window_bool, verticalspeed_bg, verticalspeed_hand, placeholder_angle);
        // There is no dedicated altimeter needle asset yet, so the airspeed needle is reused.
        GaugeWindow("altitude",       &altitude_window_bool,       altitude_bg,      airspeed_hand,      placeholder_angle);
        GaugeWindow("Compass",        &Compass_window_bool,        compass_bg,       compass_handle,     placeholder_angle);
        GaugeWindow("ROLL",           &ROLL_window_bool,           roll_bg,          roll_handle,        0.7f);
        AttitudeIndicator::ShowWindow(&Attitude_window_bool);
    }
}
