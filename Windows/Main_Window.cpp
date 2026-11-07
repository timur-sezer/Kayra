#include "Main_Window.h"
#include "Widgets_Controls.h"
#include "Attitude_Indicator.h"
#include "backend.h"
#include "imgui.h"
#include "imgui_internal.h"   // DockBuilder API
#include <algorithm>
#include <cmath>
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

    // Window titles (also used by the dock layout).
    static const char* const kAirSpeedTitle      = "Air_Speed";
    static const char* const kVerticalSpeedTitle = "Vertical_Speed";
    static const char* const kAltitudeTitle      = "altitude";
    static const char* const kCompassTitle       = "Compass";
    static const char* const kRollTitle          = "ROLL";
    static const char* const kConnectionTitle    = "Connection";
    static const char* const kHealthTitle        = "Health";
    static const char* const kBatteryTitle       = "Battery";
    static const char* const kLocationTitle      = "Location";
    static const char* const kFlightDataTitle    = "Flight Data";

    static constexpr float kPi = 3.14159265358979f;
    static constexpr float kDegToRad = kPi / 180.0f;
    static constexpr float kRadToDeg = 180.0f / kPi;

    static const ImVec4 kGood    = ImVec4(0.30f, 0.85f, 0.45f, 1.0f);
    static const ImVec4 kWarn    = ImVec4(0.95f, 0.75f, 0.20f, 1.0f);
    static const ImVec4 kBad     = ImVec4(0.95f, 0.30f, 0.30f, 1.0f);
    static const ImVec4 kNeutral = ImVec4(0.60f, 0.60f, 0.60f, 1.0f);

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

    // ------------------------------------------------------------------------------------
    // Layout
    // ------------------------------------------------------------------------------------

    // Builds the fixed default layout:
    //   +------------+-----------+-----------+-----------+-------------+
    //   | Connection | Air_Speed | Attitude  | altitude  | Battery     |
    //   |            +-----------+-----------+-----------+-------------+
    //   +------------+   ROLL    |  Compass  | Vert_Spd  | Location    |
    //   | Health     |           |           |           +-------------+
    //   |            |           |           |           | Flight Data |
    //   +------------+-----------+-----------+-----------+-------------+
    static void BuildDefaultLayout(ImGuiID dockspace_id, ImVec2 size)
    {
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, size);

        // Side columns
        ImGuiID left = 0, rest = 0, right = 0, center = 0;
        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.18f, &left, &rest);
        ImGui::DockBuilderSplitNode(rest, ImGuiDir_Right, 0.22f, &right, &center);

        ImGuiID left_top = 0, left_bottom = 0;
        ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.40f, &left_top, &left_bottom);

        ImGuiID right_top = 0, right_rest = 0, right_mid = 0, right_bottom = 0;
        ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, 0.30f, &right_top, &right_rest);
        ImGui::DockBuilderSplitNode(right_rest, ImGuiDir_Up, 0.50f, &right_mid, &right_bottom);

        // Gauge six-pack in the center
        ImGuiID top = 0, bottom = 0;
        ImGui::DockBuilderSplitNode(center, ImGuiDir_Up, 0.5f, &top, &bottom);

        ImGuiID top_left = 0, top_rest = 0, top_mid = 0, top_right = 0;
        ImGui::DockBuilderSplitNode(top, ImGuiDir_Left, 1.0f / 3.0f, &top_left, &top_rest);
        ImGui::DockBuilderSplitNode(top_rest, ImGuiDir_Left, 0.5f, &top_mid, &top_right);

        ImGuiID bottom_left = 0, bottom_rest = 0, bottom_mid = 0, bottom_right = 0;
        ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Left, 1.0f / 3.0f, &bottom_left, &bottom_rest);
        ImGui::DockBuilderSplitNode(bottom_rest, ImGuiDir_Left, 0.5f, &bottom_mid, &bottom_right);

        ImGui::DockBuilderDockWindow(kConnectionTitle,                left_top);
        ImGui::DockBuilderDockWindow(kHealthTitle,                    left_bottom);
        ImGui::DockBuilderDockWindow(kAirSpeedTitle,                  top_left);
        ImGui::DockBuilderDockWindow(AttitudeIndicator::kWindowTitle, top_mid);
        ImGui::DockBuilderDockWindow(kAltitudeTitle,                  top_right);
        ImGui::DockBuilderDockWindow(kRollTitle,                      bottom_left);
        ImGui::DockBuilderDockWindow(kCompassTitle,                   bottom_mid);
        ImGui::DockBuilderDockWindow(kVerticalSpeedTitle,             bottom_right);
        ImGui::DockBuilderDockWindow(kBatteryTitle,                   right_top);
        ImGui::DockBuilderDockWindow(kLocationTitle,                  right_mid);
        ImGui::DockBuilderDockWindow(kFlightDataTitle,                right_bottom);

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

    // ------------------------------------------------------------------------------------
    // Small helpers
    // ------------------------------------------------------------------------------------

    static float  Load(const std::atomic<float>& v)  { return v.load(); }
    static double Load(const std::atomic<double>& v) { return v.load(); }
    static bool   Valid(double v)                    { return std::isfinite(v); }
    static float  OrZero(float v)                    { return std::isfinite(v) ? v : 0.0f; }

    // Formats `v` with printf format `fmt`, or "--" if there is no data.
    static const char* Fmt(char* buf, size_t n, const char* fmt, double v)
    {
        if (!Valid(v)) snprintf(buf, n, "--");
        else           snprintf(buf, n, fmt, v);
        return buf;
    }

    static bool BeginDataTable(const char* id)
    {
        if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
            return false;
        ImGui::TableSetupColumn("Name",  ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        return true;
    }

    static void Row(const char* label, const char* value, const ImVec4* color = nullptr)
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("%s", label);
        ImGui::TableSetColumnIndex(1);
        if (color) ImGui::TextColored(*color, "%s", value);
        else       ImGui::TextUnformatted(value);
    }

    static void RowValue(const char* label, const char* fmt, double v)
    {
        char buf[64];
        Row(label, Fmt(buf, sizeof(buf), fmt, v));
    }

    // Coloured dot + label, for health flags.
    static void StatusRow(const char* label, bool ok, const char* ok_text = "OK", const char* bad_text = "NO")
    {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextDisabled("%s", label);
        ImGui::TableSetColumnIndex(1);
        const bool live = backend::mavsdk_connected;
        const ImVec4 col = !live ? kNeutral : (ok ? kGood : kBad);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float r = ImGui::GetTextLineHeight() * 0.3f;
        dl->AddCircleFilled(ImVec2(p.x + r, p.y + ImGui::GetTextLineHeight() * 0.5f), r, ImGui::GetColorU32(col));
        ImGui::Dummy(ImVec2(r * 2.0f, 0.0f));
        ImGui::SameLine();
        ImGui::TextColored(col, "%s", !live ? "--" : (ok ? ok_text : bad_text));
    }

    static ImVec4 StateColor(backend::ConnectionState s)
    {
        switch (s) {
            case backend::ConnectionState::Connected:  return kGood;
            case backend::ConnectionState::Connecting: return kWarn;
            case backend::ConnectionState::LinkLost:   return kBad;
            case backend::ConnectionState::Failed:     return kBad;
            default:                                   return kNeutral;
        }
    }

    // ------------------------------------------------------------------------------------
    // Gauges
    // ------------------------------------------------------------------------------------

    // Optional digital readout drawn on top of a gauge image.
    struct Readout {
        const char* text = nullptr;
        ImVec2 uv = ImVec2(0.5f, 0.8f);   // position inside the image, 0..1
        ImU32 color = IM_COL32_WHITE;
    };

    // Draws a square gauge (background + rotated needle) centered in a window.
    static void GaugeWindow(const char* title, bool* p_open, GLuint background, GLuint needle, float angle,
                            const Readout& readout = Readout())
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
                const ImVec2 top_left(center.x - half.x, center.y - half.y);

                ImDrawList* dl = ImGui::GetWindowDrawList();
                dl->AddImage(ToTexID(background), top_left, ImVec2(center.x + half.x, center.y + half.y));
                ImageRotated(ToTexID(needle), center, ImVec2(side, side), std::isfinite(angle) ? angle : 0.0f);

                if (readout.text) {
                    ImFont* font = ImGui::GetFont();
                    const float font_size = std::max(10.0f, side * 0.06f);
                    const ImVec2 ts = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, readout.text);
                    const ImVec2 pos(top_left.x + readout.uv.x * side - ts.x * 0.5f,
                                     top_left.y + readout.uv.y * side - ts.y * 0.5f);
                    dl->AddText(font, font_size, pos, readout.color, readout.text);
                }
            }

            // Reserve the area so ImGui's layout knows it is used.
            ImGui::Dummy(avail);
        }
        ImGui::End();
    }

    // Needle angles (radians, clockwise). Each mapping matches the printed scale on its dial;
    // at angle 0 every needle image points at the dial's zero mark.
    namespace Needle {
        // Airspeed dial: 0..24 m/s over 270 degrees.
        static float Airspeed(float v_m_s)  { return std::clamp(OrZero(v_m_s), 0.0f, 24.0f) * (270.0f / 24.0f) * kDegToRad; }
        // Vertical speed dial: zero at 9 o'clock, climb clockwise; labelled 0-3-6-9-12 every 45 degrees.
        static float Vertical(float v_m_s)  { return std::clamp(OrZero(v_m_s), -12.0f, 12.0f) * (45.0f / 3.0f) * kDegToRad; }
        // Altimeter: 0..40 m once around, 0 at 12 o'clock. Reuses the airspeed needle, which
        // points at -135 degrees when unrotated, hence the +135 degree offset.
        static float Altitude(float alt_m)  { return (std::clamp(OrZero(alt_m), 0.0f, 40.0f) * (360.0f / 40.0f) + 135.0f) * kDegToRad; }
        // Compass card rotates opposite to the heading (heading-up display).
        static float Compass(float hdg_deg) { return -OrZero(hdg_deg) * kDegToRad; }
        // Drone symbol banks with the vehicle.
        static float Roll(float roll_deg)   { return OrZero(roll_deg) * kDegToRad; }
    }

    // ------------------------------------------------------------------------------------
    // Telemetry windows
    // ------------------------------------------------------------------------------------

    static void ConnectionWindow(bool* p_open)
    {
        if (!*p_open) return;
        if (ImGui::Begin(kConnectionTitle, p_open)) {
            static char url[256] = "udpin://0.0.0.0:14540";

            const backend::ConnectionState state = backend::connection_state;
            const bool busy = state == backend::ConnectionState::Connecting
                           || state == backend::ConnectionState::Connected
                           || state == backend::ConnectionState::LinkLost;

            ImGui::TextDisabled("Connection URL");
            ImGui::SetNextItemWidth(-FLT_MIN);
            ImGui::BeginDisabled(busy);
            const bool enter = ImGui::InputText("##url", url, sizeof(url), ImGuiInputTextFlags_EnterReturnsTrue);
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("PX4 SITL:  udpin://0.0.0.0:14540\nQGC port:  udpin://0.0.0.0:14550\nSerial:    serial:///dev/ttyUSB0:57600");

            const float w = ImGui::GetContentRegionAvail().x;
            if (!busy) {
                if (ImGui::Button("Connect", ImVec2(w, 0)) || enter)
                    backend::connect(url);
            }
            else {
                if (ImGui::Button(state == backend::ConnectionState::Connecting ? "Cancel" : "Disconnect", ImVec2(w, 0)))
                    backend::disconnect();
            }

            ImGui::Spacing();
            ImGui::TextColored(StateColor(state), "%s", backend::connection_state_name(state));
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", backend::status_text().c_str());
            ImGui::PopTextWrapPos();

            ImGui::Separator();
            if (BeginDataTable("conn")) {
                const std::string mode = backend::flight_mode();
                Row("Flight mode", mode.empty() ? "--" : mode.c_str());
                StatusRow("Armed", backend::Is_armed, "ARMED", "Disarmed");
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    static void HealthWindow(bool* p_open)
    {
        if (!*p_open) return;
        if (ImGui::Begin(kHealthTitle, p_open)) {
            ImGui::SeparatorText("Sensors");
            if (BeginDataTable("health_sensors")) {
                StatusRow("Gyro calibration",  backend::Is_gyrometer_calibration_ok);
                StatusRow("Accel calibration", backend::Is_accelerometer_calibration_ok);
                StatusRow("Mag calibration",   backend::Is_magnetometer_calibration_ok);
                ImGui::EndTable();
            }
            ImGui::SeparatorText("Position");
            if (BeginDataTable("health_pos")) {
                StatusRow("Local position",  backend::Is_local_position_ok);
                StatusRow("Global position", backend::Is_global_position_ok);
                StatusRow("Home position",   backend::Is_home_position_ok);
                ImGui::EndTable();
            }
            ImGui::SeparatorText("Arming");
            if (BeginDataTable("health_arm")) {
                StatusRow("Armable", backend::Is_armable, "YES", "NO");
                StatusRow("Armed",   backend::Is_armed,   "YES", "NO");
                ImGui::EndTable();
            }
            ImGui::SeparatorText("RC link");
            if (BeginDataTable("health_rc")) {
                StatusRow("RC available", backend::Is_available, "YES", "NO");
                StatusRow("Seen once",    backend::Was_available_once, "YES", "NO");
                RowValue("Signal", "%.0f %%", Load(backend::Signal_strength_percent));
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    static void BatteryWindow(bool* p_open)
    {
        if (!*p_open) return;
        if (ImGui::Begin(kBatteryTitle, p_open)) {
            const float pct = Load(backend::Battery_remaining_percent);
            char overlay[32];
            Fmt(overlay, sizeof(overlay), "%.0f %%", pct);

            const ImVec4 col = !Valid(pct) ? kNeutral : (pct > 50.0f ? kGood : (pct > 20.0f ? kWarn : kBad));
            ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
            ImGui::ProgressBar(Valid(pct) ? std::clamp(pct / 100.0f, 0.0f, 1.0f) : 0.0f,
                               ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 1.6f), overlay);
            ImGui::PopStyleColor();

            if (BeginDataTable("battery")) {
                RowValue("Voltage",     "%.2f V",  Load(backend::Battery_voltage));
                RowValue("Current",     "%.2f A",  Load(backend::Battery_current_battery));
                RowValue("Consumed",    "%.3f Ah", Load(backend::Battery_capacity_consumed));
                RowValue("Temperature", "%.1f C",  Load(backend::Battery_temperature_deg));

                const float t = Load(backend::Battery_time_remaining);
                char buf[32];
                if (Valid(t) && t >= 0.0f) snprintf(buf, sizeof(buf), "%d:%02d", (int)t / 60, (int)t % 60);
                else                       snprintf(buf, sizeof(buf), "--");
                Row("Time left", buf);
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    static void LocationWindow(bool* p_open)
    {
        if (!*p_open) return;
        if (ImGui::Begin(kLocationTitle, p_open)) {
            if (BeginDataTable("location")) {
                RowValue("Latitude",      "%.7f",   Load(backend::Position_latitude));
                RowValue("Longitude",     "%.7f",   Load(backend::Position_longitude));
                RowValue("Altitude AMSL", "%.1f m", Load(backend::Position_altitude));
                RowValue("Altitude rel.", "%.1f m", Load(backend::Position_relative_altitude));

                const int fix = backend::GPS_fix_type;
                const ImVec4 fix_col = !backend::mavsdk_connected ? kNeutral : (fix >= 3 ? kGood : (fix == 2 ? kWarn : kBad));
                char buf[48];
                if (backend::mavsdk_connected)
                    snprintf(buf, sizeof(buf), "%s (%d sats)", backend::gps_fix_name(fix), backend::GPS_num_satellites.load());
                else
                    snprintf(buf, sizeof(buf), "--");
                Row("GPS", buf, &fix_col);
                ImGui::EndTable();
            }

            ImGui::SeparatorText("Velocity (NED)");
            if (BeginDataTable("velocity")) {
                const float vn = Load(backend::Velocity_x_m_s);
                const float ve = Load(backend::Velocity_y_m_s);
                const float vd = Load(backend::Velocity_z_m_s);
                RowValue("North", "%.2f m/s", vn);
                RowValue("East",  "%.2f m/s", ve);
                RowValue("Down",  "%.2f m/s", vd);
                RowValue("Horizontal", "%.2f m/s", std::sqrt(vn * vn + ve * ve));
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    static void FlightDataWindow(bool* p_open)
    {
        if (!*p_open) return;
        if (ImGui::Begin(kFlightDataTitle, p_open)) {
            ImGui::SeparatorText("Attitude");
            if (BeginDataTable("attitude")) {
                RowValue("Roll",    "%.1f deg", Load(backend::Attitude_roll_deg));
                RowValue("Pitch",   "%.1f deg", Load(backend::Attitude_pitch_deg));
                RowValue("Yaw",     "%.1f deg", Load(backend::Attitude_yaw_deg));
                RowValue("Heading", "%.0f deg", Load(backend::Heading_deg));
                ImGui::EndTable();
            }
            ImGui::SeparatorText("Angular rate");
            if (BeginDataTable("rates")) {
                RowValue("Roll rate",  "%.1f deg/s", Load(backend::Angular_velocity_roll_rad)  * kRadToDeg);
                RowValue("Pitch rate", "%.1f deg/s", Load(backend::Angular_velocity_pitch_rad) * kRadToDeg);
                RowValue("Yaw rate",   "%.1f deg/s", Load(backend::Angular_velocity_yaw_rad)   * kRadToDeg);
                ImGui::EndTable();
            }
            ImGui::SeparatorText("Speed");
            if (BeginDataTable("speeds")) {
                RowValue("Airspeed",    "%.1f m/s", Load(backend::Airspeed_m_s));
                RowValue("Groundspeed", "%.1f m/s", Load(backend::Groundspeed_m_s));
                RowValue("Climb rate",  "%.2f m/s", Load(backend::Climb_rate_m_s));
                RowValue("Throttle",    "%.0f %%",  Load(backend::Throttle_percent));
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    // ------------------------------------------------------------------------------------
    // Frame
    // ------------------------------------------------------------------------------------

    void Mycode() {
        static bool ROLL_window_bool = true;
        static bool Attitude_window_bool = true;
        static bool Air_Speed_window_bool = true;
        static bool Vertical_Speed_window_bool = true;
        static bool altitude_window_bool = true;
        static bool Compass_window_bool = true;
        static bool Connection_window_bool = true;
        static bool Health_window_bool = true;
        static bool BATTERY_window_bool = true;
        static bool LOCATION_window_bool = true;
        static bool FlightData_window_bool = true;

        ShowMainDockSpace();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("OPEN-WINDOW"))
            {
                ImGui::SeparatorText("Gauges");
                ImGui::MenuItem("Air Speed",      nullptr, &Air_Speed_window_bool);
                ImGui::MenuItem("Attitude",       nullptr, &Attitude_window_bool);
                ImGui::MenuItem("Altitude",       nullptr, &altitude_window_bool);
                ImGui::MenuItem("Roll",           nullptr, &ROLL_window_bool);
                ImGui::MenuItem("Compass",        nullptr, &Compass_window_bool);
                ImGui::MenuItem("Vertical Speed", nullptr, &Vertical_Speed_window_bool);
                ImGui::SeparatorText("Telemetry");
                ImGui::MenuItem("Connection",     nullptr, &Connection_window_bool);
                ImGui::MenuItem("Health",         nullptr, &Health_window_bool);
                ImGui::MenuItem("Battery",        nullptr, &BATTERY_window_bool);
                ImGui::MenuItem("Location",       nullptr, &LOCATION_window_bool);
                ImGui::MenuItem("Flight Data",    nullptr, &FlightData_window_bool);
                ImGui::EndMenu();
            }

            // Connection status in the menu bar, left of the Settings button.
            const backend::ConnectionState state = backend::connection_state;
            const char* state_name = backend::connection_state_name(state);
            const float status_w = ImGui::CalcTextSize(state_name).x;
            ImGui::SetCursorPosX(window_size.x - 140 - status_w);
            ImGui::TextColored(StateColor(state), "%s", state_name);

            ImGui::SetCursorPos(ImVec2(window_size.x - 120, 3));
            ImGui::Button("Settings", ImVec2(100, 25));
            ImGui::EndMainMenuBar();
        }

        // ---- Read telemetry once per frame ----
        const bool  live      = backend::mavsdk_connected;
        const float roll      = Load(backend::Attitude_roll_deg);
        const float pitch     = Load(backend::Attitude_pitch_deg);
        float heading         = Load(backend::Heading_deg);
        if (!Valid(heading)) heading = Load(backend::Attitude_yaw_deg);
        float airspeed        = Load(backend::Airspeed_m_s);
        if (!Valid(airspeed)) airspeed = Load(backend::Groundspeed_m_s);
        float climb           = Load(backend::Climb_rate_m_s);
        if (!Valid(climb) && Valid(Load(backend::Velocity_z_m_s))) climb = -Load(backend::Velocity_z_m_s);
        const float rel_alt   = Load(backend::Position_relative_altitude);

        if (live && Valid(roll) && Valid(pitch))
            AttitudeIndicator::SetAttitude(pitch, roll);

        // ---- Gauges ----
        char airspeed_txt[16], climb_txt[16], alt_txt[24], heading_txt[16], roll_txt[24];
        Fmt(airspeed_txt, sizeof(airspeed_txt), "%.1f", airspeed);
        Fmt(climb_txt,    sizeof(climb_txt),    "%+.1f", climb);
        Fmt(alt_txt,      sizeof(alt_txt),      "%.1f m", rel_alt);
        Fmt(heading_txt,  sizeof(heading_txt),  "%03.0f", Valid(heading) ? std::fmod(heading + 360.0f, 360.0f) : heading);
        Fmt(roll_txt,     sizeof(roll_txt),     "%+.1f deg", roll);

        // Readout positions are measured from the dial images (fractions of the image size).
        const ImU32 dark  = IM_COL32(20, 25, 35, 255);
        const ImU32 light = IM_COL32(230, 240, 245, 255);
        GaugeWindow(kAirSpeedTitle,      &Air_Speed_window_bool,      airspeed_bg,      airspeed_hand,
                    Needle::Airspeed(airspeed), { airspeed_txt, ImVec2(0.495f, 0.793f), dark });
        GaugeWindow(kVerticalSpeedTitle, &Vertical_Speed_window_bool, verticalspeed_bg, verticalspeed_hand,
                    Needle::Vertical(climb),    { climb_txt,    ImVec2(0.503f, 0.781f), dark });
        // There is no dedicated altimeter needle asset yet, so the airspeed needle is reused.
        GaugeWindow(kAltitudeTitle,      &altitude_window_bool,       altitude_bg,      airspeed_hand,
                    Needle::Altitude(rel_alt),  { alt_txt,      ImVec2(0.500f, 0.750f), light });
        GaugeWindow(kCompassTitle,       &Compass_window_bool,        compass_bg,       compass_handle,
                    Needle::Compass(heading),   { heading_txt,  ImVec2(0.500f, 0.740f), light });
        GaugeWindow(kRollTitle,          &ROLL_window_bool,           roll_bg,          roll_handle,
                    Needle::Roll(roll),         { roll_txt,     ImVec2(0.500f, 0.800f), light });
        AttitudeIndicator::ShowWindow(&Attitude_window_bool);

        // ---- Telemetry windows ----
        ConnectionWindow(&Connection_window_bool);
        HealthWindow(&Health_window_bool);
        BatteryWindow(&BATTERY_window_bool);
        LocationWindow(&LOCATION_window_bool);
        FlightDataWindow(&FlightData_window_bool);
    }
}
