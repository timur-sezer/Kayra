#include "Settings.h"
#include "Theme.h"

#include "Main_Window.h"
#include "Mission_Plan.h"
#include "backend.h"
#include "MapWidget.h"
#include "imgui.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace Settings {
    namespace {
        Values      g_values;
        std::string g_saved;        // serialized form of what is on disk
        bool        g_style_dirty = false;

        // Calls f(key, field) for every stored field; used for both reading and writing.
        template <class F>
        void Visit(Values& v, F&& f)
        {
            f("theme", v.theme);
            f("ui_scale", v.ui_scale);
            f("rounding", v.rounding);
            f("custom_accent", v.custom_accent);
            f("accent_r", v.accent[0]);
            f("accent_g", v.accent[1]);
            f("accent_b", v.accent[2]);
            f("connection_url", v.connection_url);
            f("auto_connect", v.auto_connect);
            f("tile_source", v.tile_source);
            f("show_trail", v.show_trail);
            f("follow_on_connect", v.follow_on_connect);
            f("default_altitude_m", v.default_altitude_m);
            f("first_item_takeoff", v.first_item_takeoff);
            f("plan_speed_m_s", v.plan_speed_m_s);
            f("window.air_speed", v.windows.air_speed);
            f("window.attitude", v.windows.attitude);
            f("window.altitude", v.windows.altitude);
            f("window.roll", v.windows.roll);
            f("window.compass", v.windows.compass);
            f("window.vertical_speed", v.windows.vertical_speed);
            f("window.connection", v.windows.connection);
            f("window.health", v.windows.health);
            f("window.battery", v.windows.battery);
            f("window.location", v.windows.location);
            f("window.flight_data", v.windows.flight_data);
            f("window.map", v.windows.map);
            f("window.terminal", v.windows.terminal);
            f("last_view", v.last_view);
            f("window.plan_map", v.plan_map);
            f("window.plan_node_editor", v.plan_graph);
            f("window.plan_mission", v.plan_panel);
            f("window.plan_terminal", v.plan_terminal);
        }

        struct Writer {
            std::ostringstream& out;
            void operator()(const char* k, int v)   const { out << k << '=' << v << '\n'; }
            void operator()(const char* k, float v) const { out << k << '=' << v << '\n'; }
            void operator()(const char* k, bool v)  const { out << k << '=' << (v ? 1 : 0) << '\n'; }
            void operator()(const char* k, const char (&v)[256]) const { out << k << '=' << v << '\n'; }
        };

        struct Reader {
            const std::string& key;
            const std::string& value;
            void operator()(const char* k, int& v)   const { if (key == k) v = std::atoi(value.c_str()); }
            void operator()(const char* k, float& v) const { if (key == k) v = float(std::atof(value.c_str())); }
            void operator()(const char* k, bool& v)  const { if (key == k) v = value == "1" || value == "true"; }
            void operator()(const char* k, char (&v)[256]) const { if (key == k) snprintf(v, sizeof(v), "%s", value.c_str()); }
        };

        std::string Serialize(Values& v)
        {
            std::ostringstream out;
            out << "# KAYRA settings (written automatically)\n";
            Visit(v, Writer{out});
            return out.str();
        }

        void Sanitize(Values& v)
        {
            v.theme = std::clamp(v.theme, 0, ThemeCount - 1);
            v.ui_scale = std::clamp(v.ui_scale, 0.75f, 1.5f);
            v.rounding = std::clamp(v.rounding, 0.0f, 12.0f);
            v.tile_source = std::clamp(v.tile_source, 0, MapWidget::sourceCount() - 1);
            v.default_altitude_m = std::clamp(v.default_altitude_m, 1.0f, 1000.0f);
            v.plan_speed_m_s = std::clamp(v.plan_speed_m_s, 0.5f, 100.0f);
            v.last_view = std::clamp(v.last_view, 0, 2);
            for (float& c : v.accent) c = std::clamp(c, 0.0f, 1.0f);
        }

        // ---- Settings tab helpers ----

        void HelpMarker(const char* text)
        {
            ImGui::SameLine();
            ImGui::TextDisabled("(?)");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", text);
        }

        // Label on the left, widget on the right, like a form.
        void FieldLabel(const char* label)
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::SameLine(ImGui::GetContentRegionMax().x * 0.42f);
            ImGui::SetNextItemWidth(std::min(360.0f * Theme::Scale(),
                                             ImGui::GetContentRegionAvail().x));
        }

        std::string FormatBytes(uintmax_t bytes)
        {
            char buf[32];
            if (bytes >= (1u << 30))      snprintf(buf, sizeof(buf), "%.2f GB", bytes / double(1u << 30));
            else if (bytes >= (1u << 20)) snprintf(buf, sizeof(buf), "%.1f MB", bytes / double(1u << 20));
            else if (bytes >= 1024)       snprintf(buf, sizeof(buf), "%.0f KB", bytes / 1024.0);
            else                          snprintf(buf, sizeof(buf), "%u B", unsigned(bytes));
            return buf;
        }

        struct CacheInfo { bool known = false; uintmax_t bytes = 0; size_t files = 0; };

        CacheInfo MeasureCache()
        {
            CacheInfo info;
            std::error_code ec;
            const fs::path dir = MissionPlan::TileCacheDir();
            info.known = true;
            if (!fs::exists(dir, ec))
                return info;
            for (auto it = fs::recursive_directory_iterator(dir, ec); !ec && it != fs::recursive_directory_iterator(); it.increment(ec)) {
                if (it->is_regular_file(ec)) {
                    info.bytes += it->file_size(ec);
                    ++info.files;
                }
            }
            return info;
        }

        // ---- Sections ----

        void AppearanceSection(Values& v)
        {
            Theme::Section("Theme");
            static const char* kThemes[] = {"KAYRA Dark", "KAYRA Light", "ImGui Dark", "ImGui Classic"};
            FieldLabel("Color theme");
            g_style_dirty |= ImGui::Combo("##theme", &v.theme, kThemes, ThemeCount);

            FieldLabel("Accent color");
            g_style_dirty |= ImGui::Checkbox("Custom##accent", &v.custom_accent);
            ImGui::SameLine();
            ImGui::BeginDisabled(!v.custom_accent);
            g_style_dirty |= ImGui::ColorEdit3("##accent", v.accent, ImGuiColorEditFlags_NoInputs);
            ImGui::EndDisabled();
            HelpMarker("Used for checkboxes, sliders, selected tabs and highlights");

            Theme::Section("Size");
            FieldLabel("UI scale");
            // Applied when the slider is released, so the slider doesn't move under the mouse.
            static float pending_scale = -1.0f;
            if (pending_scale < 0.0f) pending_scale = v.ui_scale;
            ImGui::SliderFloat("##scale", &pending_scale, 0.75f, 1.5f, "%.2fx");
            if (ImGui::IsItemDeactivatedAfterEdit()) {
                v.ui_scale = pending_scale;
                g_style_dirty = true;
            }
            if (!ImGui::IsItemActive()) pending_scale = v.ui_scale;

            FieldLabel("Corner rounding");
            g_style_dirty |= ImGui::SliderFloat("##rounding", &v.rounding, 0.0f, 12.0f, "%.0f px");

            ImGui::Spacing();
            if (ImGui::Button("Reset appearance")) {
                const Values d;
                v.theme = d.theme; v.ui_scale = d.ui_scale; v.rounding = d.rounding;
                v.custom_accent = d.custom_accent;
                std::memcpy(v.accent, d.accent, sizeof(v.accent));
                pending_scale = v.ui_scale;
                g_style_dirty = true;
            }
        }

        void ConnectionSection(Values& v)
        {
            Theme::Section("Vehicle link");
            FieldLabel("Connection URL");
            if (ImGui::InputText("##url", v.connection_url, sizeof(v.connection_url)))
                MyWindow::SetConnectionUrl(v.connection_url);
            HelpMarker("PX4 SITL:  udpin://0.0.0.0:14540\nGCS port:  udpin://0.0.0.0:14550\n"
                       "Serial:    serial:///dev/ttyUSB0:57600\nWindows:   serial://COM3:57600");

            FieldLabel("Connect on startup");
            ImGui::Checkbox("##auto", &v.auto_connect);
            HelpMarker("Connect to this URL when KAYRA starts. A URL given on the command line takes precedence.");

            Theme::Section("Status");
            const backend::ConnectionState state = backend::connection_state;
            FieldLabel("State");
            ImGui::TextUnformatted(backend::connection_state_name(state));
            FieldLabel("Details");
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", backend::status_text().c_str());
            ImGui::PopTextWrapPos();

            const bool busy = state == backend::ConnectionState::Connecting
                           || state == backend::ConnectionState::Connected
                           || state == backend::ConnectionState::LinkLost;
            ImGui::Spacing();
            if (!busy) {
                ImGui::BeginDisabled(!backend::has_mavsdk());
                if (Theme::PrimaryButton("Connect now"))
                    backend::connect(v.connection_url);
                ImGui::EndDisabled();
            }
            else if (Theme::DangerButton("Disconnect")) {
                backend::disconnect();
            }
            if (!backend::has_mavsdk())
                ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.2f, 1.0f), "KAYRA was built without MAVSDK, so it cannot connect.");
        }

        void MapSection(Values& v)
        {
            Theme::Section("Display");
            FieldLabel("Map tiles");
            if (ImGui::BeginCombo("##tiles", MapWidget::sourceAt(v.tile_source).name)) {
                for (int i = 0; i < MapWidget::sourceCount(); ++i)
                    if (ImGui::Selectable(MapWidget::sourceAt(i).name, i == v.tile_source))
                        v.tile_source = i;
                ImGui::EndCombo();
            }
            FieldLabel("Show flight trail");
            ImGui::Checkbox("##trail", &v.show_trail);
            FieldLabel("Follow vehicle on connect");
            ImGui::Checkbox("##follow", &v.follow_on_connect);
            HelpMarker("Center the map on the vehicle when its first position arrives");

            Theme::Section("Offline tile cache");
            static CacheInfo cache;
            if (!cache.known)
                cache = MeasureCache();
            FieldLabel("Location");
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", MissionPlan::TileCacheDir().c_str());
            ImGui::PopTextWrapPos();
            FieldLabel("Size");
            ImGui::Text("%s in %zu tiles", FormatBytes(cache.bytes).c_str(), cache.files);

            ImGui::Spacing();
            if (ImGui::Button("Refresh"))
                cache = MeasureCache();
            ImGui::SameLine();
            if (Theme::DangerButton("Clear cache..."))
                ImGui::OpenPopup("Clear tile cache?");
            if (ImGui::BeginPopupModal("Clear tile cache?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Delete %s of downloaded map tiles?", FormatBytes(cache.bytes).c_str());
                ImGui::TextDisabled("Areas will be downloaded again when you view them.");
                ImGui::Spacing();
                if (ImGui::Button("Delete", ImVec2(120, 0))) {
                    MissionPlan::ClearTileCache();
                    cache = MeasureCache();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }

        void MissionSection(Values& v)
        {
            Theme::Section("New waypoints");
            FieldLabel("Default altitude");
            ImGui::DragFloat("##alt", &v.default_altitude_m, 0.5f, 1.0f, 1000.0f, "%.1f m");
            HelpMarker("Altitude above home for waypoints added on the map");
            FieldLabel("Start with takeoff");
            ImGui::Checkbox("##takeoff", &v.first_item_takeoff);
            HelpMarker("The first waypoint added to an empty plan becomes a takeoff item");

            Theme::Section("Estimates");
            FieldLabel("Cruise speed");
            ImGui::DragFloat("##speed", &v.plan_speed_m_s, 0.1f, 0.5f, 100.0f, "%.1f m/s");
            HelpMarker("Used for the flight time estimate in the Mission Plan tab");
        }

        void FlightViewSection(Values& v)
        {
            Theme::Section("Gauges");
            ImGui::Checkbox("Air speed", &v.windows.air_speed);
            ImGui::Checkbox("Attitude", &v.windows.attitude);
            ImGui::Checkbox("Altitude", &v.windows.altitude);
            ImGui::Checkbox("Roll", &v.windows.roll);
            ImGui::Checkbox("Compass", &v.windows.compass);
            ImGui::Checkbox("Vertical speed", &v.windows.vertical_speed);
            Theme::Section("Telemetry");
            ImGui::Checkbox("Connection", &v.windows.connection);
            ImGui::Checkbox("Health", &v.windows.health);
            ImGui::Checkbox("Battery", &v.windows.battery);
            ImGui::Checkbox("Location", &v.windows.location);
            ImGui::Checkbox("Flight data", &v.windows.flight_data);
            ImGui::Checkbox("Map##flight", &v.windows.map);
            ImGui::Checkbox("Terminal##flight", &v.windows.terminal);

            Theme::Section("Flight layout");
            if (ImGui::Button("Default Flight windows"))
                v.windows = FlightWindows();
            ImGui::SameLine();
            if (ImGui::Button("Reset Flight layout"))
                MyWindow::ResetLayout();
            HelpMarker("Puts every Flight window back in its default place");

            Theme::Section("Mission Plan windows");
            ImGui::Checkbox("Map", &v.plan_map);
            ImGui::Checkbox("Node editor", &v.plan_graph);
            ImGui::Checkbox("Mission panel", &v.plan_panel);
            ImGui::Checkbox("Terminal##plan", &v.plan_terminal);
            if (ImGui::Button("Reset Mission Plan layout")) {
                v.plan_map = v.plan_graph = v.plan_panel = v.plan_terminal = true;
                MissionPlan::ResetLayout();
            }
        }

        void AboutSection()
        {
            Theme::Section("KAYRA Ground Control Station");
            FieldLabel("MAVSDK");
            ImGui::TextUnformatted(backend::has_mavsdk() ? "Available" : "Not built in");
            FieldLabel("Dear ImGui");
            ImGui::TextUnformatted(ImGui::GetVersion());
            FieldLabel("Settings file");
            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", FilePath().c_str());
            ImGui::PopTextWrapPos();

            Theme::Section("Reset");
            if (Theme::DangerButton("Reset all settings..."))
                ImGui::OpenPopup("Reset all settings?");
            if (ImGui::BeginPopupModal("Reset all settings?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("Restore every setting to its default value?");
                ImGui::Spacing();
                if (ImGui::Button("Reset", ImVec2(120, 0))) {
                    const int view = g_values.last_view;
                    g_values = Values();
                    g_values.last_view = view;
                    MyWindow::SetConnectionUrl(g_values.connection_url);
                    MyWindow::ResetLayout();
                    MissionPlan::ResetLayout();
                    g_style_dirty = true;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }
    }

    Values& Get() { return g_values; }

    std::string FilePath()
    {
#if defined(_WIN32)
        const char* base = std::getenv("APPDATA");
        return base ? std::string(base) + "/KAYRA/settings.ini" : "kayra_settings.ini";
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Library/Application Support/KAYRA/settings.ini" : "kayra_settings.ini";
#else
        const char* xdg = std::getenv("XDG_CONFIG_HOME");
        if (xdg && *xdg) return std::string(xdg) + "/KAYRA/settings.ini";
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/.config/KAYRA/settings.ini" : "kayra_settings.ini";
#endif
    }

    void Load()
    {
        std::ifstream in(FilePath());
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty() || line[0] == '#')
                continue;
            const size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;
            const std::string key = line.substr(0, eq);
            const std::string value = line.substr(eq + 1);
            Visit(g_values, Reader{key, value});
        }
        Sanitize(g_values);
        g_saved = Serialize(g_values);
    }

    void SaveIfChanged()
    {
        if (g_style_dirty) {
            ApplyStyle();
            g_style_dirty = false;
        }
        Sanitize(g_values);
        const std::string now = Serialize(g_values);
        if (now == g_saved)
            return;

        std::error_code ec;
        const fs::path path = FilePath();
        fs::create_directories(path.parent_path(), ec);
        std::ofstream out(path, std::ios::trunc);
        out << now;
        g_saved = now;  // even if writing failed: don't retry every frame, the values still apply in memory
    }

    void ApplyStyle()
    {
        Theme::Apply(g_values.theme, g_values.ui_scale, g_values.rounding,
                     g_values.custom_accent ? g_values.accent : nullptr);
    }

    void ShowView()
    {
        Values& v = g_values;
        static int section = 0;
        static const char* kSections[] = {"Appearance", "Connection", "Map", "Mission planning", "Windows", "About"};

        const float nav_w = 220.0f * Theme::Scale();
        ImGui::BeginChild("##settings_nav", ImVec2(nav_w, 0), true);
        ImGui::TextDisabled("SETTINGS");
        ImGui::Spacing();
        for (int i = 0; i < IM_ARRAYSIZE(kSections); ++i)
            if (ImGui::Selectable(kSections[i], section == i, 0, ImVec2(0, ImGui::GetFrameHeight())))
                section = i;
        ImGui::EndChild();

        ImGui::SameLine();
        ImGui::BeginChild("##settings_page", ImVec2(0, 0), true);
        {
            Theme::FontScope heading(Theme::Font::Heading);
            ImGui::TextUnformatted(kSections[section]);
        }
        switch (section) {
            case 0: AppearanceSection(v); break;
            case 1: ConnectionSection(v); break;
            case 2: MapSection(v); break;
            case 3: MissionSection(v); break;
            case 4: FlightViewSection(v); break;
            default: AboutSection(); break;
        }
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Changes are applied and saved automatically.");
        ImGui::EndChild();
    }
}
