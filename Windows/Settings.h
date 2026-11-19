#pragma once

// User settings: stored in a small key=value file and saved automatically when they change.
//   macOS:   ~/Library/Application Support/KAYRA/settings.ini
//   Linux:   ~/.config/KAYRA/settings.ini
//   Windows: %APPDATA%\KAYRA\settings.ini

#include <string>

namespace Settings {
    enum ThemeId : int { ThemeKayraDark = 0, ThemeKayraLight, ThemeImGuiDark, ThemeClassic, ThemeCount };

    struct FlightWindows {
        // Health is closed in the default layout; it opens as a tab behind Connection.
        bool air_speed = true, attitude = true, altitude = true, roll = true, compass = true, vertical_speed = true;
        bool connection = true, health = false, battery = true, location = true, flight_data = true;
        bool map = true;
        bool terminal = true;  // tab next to Flight Data
    };

    struct Values {
        // Appearance
        int   theme = ThemeKayraDark;
        float ui_scale = 1.0f;           // 0.75 .. 1.5
        float rounding = 8.0f;           // window/frame corner radius, px
        bool  custom_accent = false;
        float accent[3] = {0.04f, 0.52f, 1.0f};

        // Connection
        char  connection_url[256] = "udpin://0.0.0.0:14540";
        bool  auto_connect = false;      // connect to connection_url on startup

        // Map
        int   tile_source = 0;           // index into MapWidget::sourceAt()
        bool  show_trail = true;
        bool  follow_on_connect = true;  // center on the vehicle when its first position arrives

        // Mission planning
        float default_altitude_m = 30.0f;  // for new waypoints (relative to home)
        bool  first_item_takeoff = true;   // the first waypoint of an empty plan is a takeoff
        float plan_speed_m_s = 10.0f;      // used for the flight time estimate

        // Flight view
        FlightWindows windows;
        int   last_view = 0;             // tab that was open when KAYRA was closed

        // Mission Plan windows
        bool  plan_map = true, plan_graph = true, plan_panel = true;
        bool  plan_terminal = true;  // tab next to Mission
    };

    Values& Get();

    void Load();             // read the settings file (missing keys keep their defaults)
    void SaveIfChanged();    // call once per frame; writes the file only when something changed
    void ApplyStyle();       // apply theme, scale, rounding and accent to ImGui
    std::string FilePath();

    void ShowView();         // contents of the Settings tab (inside an already open window)
}
