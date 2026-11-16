#include "backend.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>

#ifdef KAYRA_HAS_MAVSDK
// MAVSDK 4 renamed its headers from .h to .hpp; support both (Docker image uses v3).
#if __has_include(<mavsdk/mavsdk.hpp>)
#include <mavsdk/mavsdk.hpp>
#include <mavsdk/plugins/telemetry/telemetry.hpp>
#include <mavsdk/plugins/mission_raw/mission_raw.hpp>
#else
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/telemetry/telemetry.h>
#include <mavsdk/plugins/mission_raw/mission_raw.h>
#endif
#endif

namespace backend {
    namespace {
        constexpr float  kNaNf = NAN;
        constexpr double kNaN  = NAN;
    }

    std::atomic<ConnectionState> connection_state{ConnectionState::Disconnected};
    std::atomic<bool> mavsdk_connected{false};

    std::atomic<float> Battery_temperature_deg{kNaNf};
    std::atomic<float> Battery_voltage{kNaNf};
    std::atomic<float> Battery_current_battery{kNaNf};
    std::atomic<float> Battery_capacity_consumed{kNaNf};
    std::atomic<float> Battery_remaining_percent{kNaNf};
    std::atomic<float> Battery_time_remaining{kNaNf};

    std::atomic<double> Position_latitude{kNaN};
    std::atomic<double> Position_longitude{kNaN};
    std::atomic<float>  Position_altitude{kNaNf};
    std::atomic<float>  Position_relative_altitude{kNaNf};

    std::atomic<float> Attitude_roll_deg{kNaNf};
    std::atomic<float> Attitude_pitch_deg{kNaNf};
    std::atomic<float> Attitude_yaw_deg{kNaNf};
    std::atomic<float> Heading_deg{kNaNf};

    std::atomic<float> Angular_velocity_roll_rad{kNaNf};
    std::atomic<float> Angular_velocity_pitch_rad{kNaNf};
    std::atomic<float> Angular_velocity_yaw_rad{kNaNf};

    std::atomic<float> Velocity_x_m_s{kNaNf};
    std::atomic<float> Velocity_y_m_s{kNaNf};
    std::atomic<float> Velocity_z_m_s{kNaNf};
    std::atomic<float> Airspeed_m_s{kNaNf};
    std::atomic<float> Groundspeed_m_s{kNaNf};
    std::atomic<float> Climb_rate_m_s{kNaNf};
    std::atomic<float> Throttle_percent{kNaNf};

    std::atomic<int> GPS_fix_type{0};
    std::atomic<int> GPS_num_satellites{0};

    std::atomic<double> Home_latitude{kNaN};
    std::atomic<double> Home_longitude{kNaN};
    std::atomic<float>  Home_altitude_amsl{kNaNf};

    std::atomic<int> Mission_current{-1};
    std::atomic<int> Mission_total{0};
    std::atomic<int> Mission_version{0};

    std::atomic<bool> Is_gyrometer_calibration_ok{false};
    std::atomic<bool> Is_accelerometer_calibration_ok{false};
    std::atomic<bool> Is_magnetometer_calibration_ok{false};
    std::atomic<bool> Is_local_position_ok{false};
    std::atomic<bool> Is_global_position_ok{false};
    std::atomic<bool> Is_home_position_ok{false};
    std::atomic<bool> Is_armable{false};
    std::atomic<bool> Is_armed{false};

    std::atomic<bool>  Was_available_once{false};
    std::atomic<bool>  Is_available{false};
    std::atomic<float> Signal_strength_percent{kNaNf};

    namespace {
        std::mutex  g_text_mutex;
        std::string g_status = "Not connected";
        std::string g_flight_mode;

        void set_status(const std::string& s)
        {
            {
                std::lock_guard<std::mutex> lock(g_text_mutex);
                g_status = s;
            }
            std::cout << "[backend] " << s << std::endl;
        }

        // Mission data (written by MAVSDK callbacks, read by the GUI)
        std::mutex                   g_mission_mutex;
        std::vector<MissionItem>     g_mission;
        std::string                  g_mission_status = "No mission";

        [[maybe_unused]] void set_mission_status(const std::string& s)
        {
            std::lock_guard<std::mutex> lock(g_mission_mutex);
            g_mission_status = s;
        }

        void store_mission(std::vector<MissionItem> items, const std::string& status)
        {
            const int total = int(items.size());
            {
                std::lock_guard<std::mutex> lock(g_mission_mutex);
                g_mission = std::move(items);
                g_mission_status = status;
            }
            Mission_total = total;
            ++Mission_version;
        }

        [[maybe_unused]] bool is_global_frame(int frame)
        {
            // MAV_FRAME_GLOBAL(_INT), _RELATIVE_ALT(_INT), _TERRAIN_ALT(_INT)
            return frame == 0 || frame == 3 || frame == 5 || frame == 6 || frame == 10 || frame == 11;
        }

        [[maybe_unused]] std::string describe(const std::vector<MissionItem>& items)
        {
            if (items.empty())
                return "No mission on vehicle";
            size_t positional = 0;
            for (const auto& it : items) positional += it.positional;
            std::ostringstream ss;
            ss << positional << " waypoints (" << items.size() << " items)";
            return ss.str();
        }

        void set_state(ConnectionState s)
        {
            connection_state = s;
            mavsdk_connected = (s == ConnectionState::Connected);
        }

        void reset_values()
        {
            for (auto* v : { &Battery_temperature_deg, &Battery_voltage, &Battery_current_battery,
                             &Battery_capacity_consumed, &Battery_remaining_percent, &Battery_time_remaining,
                             &Position_altitude, &Position_relative_altitude,
                             &Attitude_roll_deg, &Attitude_pitch_deg, &Attitude_yaw_deg, &Heading_deg,
                             &Angular_velocity_roll_rad, &Angular_velocity_pitch_rad, &Angular_velocity_yaw_rad,
                             &Velocity_x_m_s, &Velocity_y_m_s, &Velocity_z_m_s,
                             &Airspeed_m_s, &Groundspeed_m_s, &Climb_rate_m_s, &Throttle_percent,
                             &Signal_strength_percent })
                v->store(kNaNf);
            Position_latitude = kNaN;
            Position_longitude = kNaN;
            Home_latitude = kNaN;
            Home_longitude = kNaN;
            Home_altitude_amsl = kNaNf;
            Mission_current = -1;
            store_mission({}, "No mission");
            GPS_fix_type = 0;
            GPS_num_satellites = 0;
            for (auto* b : { &Is_gyrometer_calibration_ok, &Is_accelerometer_calibration_ok,
                             &Is_magnetometer_calibration_ok, &Is_local_position_ok, &Is_global_position_ok,
                             &Is_home_position_ok, &Is_armable, &Is_armed, &Was_available_once, &Is_available })
                b->store(false);
            std::lock_guard<std::mutex> lock(g_text_mutex);
            g_flight_mode.clear();
        }
    }

    std::string status_text()
    {
        std::lock_guard<std::mutex> lock(g_text_mutex);
        return g_status;
    }

    std::string flight_mode()
    {
        std::lock_guard<std::mutex> lock(g_text_mutex);
        return g_flight_mode;
    }

    bool is_positional_command(int command)
    {
        switch (command) {
            case 16:  // NAV_WAYPOINT
            case 17:  // NAV_LOITER_UNLIM
            case 18:  // NAV_LOITER_TURNS
            case 19:  // NAV_LOITER_TIME
            case 21:  // NAV_LAND
            case 22:  // NAV_TAKEOFF
            case 31:  // NAV_LOITER_TO_ALT
            case 82:  // NAV_SPLINE_WAYPOINT
            case 84:  // NAV_VTOL_TAKEOFF
            case 85:  // NAV_VTOL_LAND
                return true;
        }
        return false;
    }

    std::vector<MissionItem> mission_items()
    {
        std::lock_guard<std::mutex> lock(g_mission_mutex);
        return g_mission;
    }

    std::string mission_status()
    {
        std::lock_guard<std::mutex> lock(g_mission_mutex);
        return g_mission_status;
    }

    const char* connection_state_name(ConnectionState s)
    {
        switch (s) {
            case ConnectionState::Disconnected: return "Disconnected";
            case ConnectionState::Connecting:   return "Connecting...";
            case ConnectionState::Connected:    return "Connected";
            case ConnectionState::LinkLost:     return "Link lost";
            case ConnectionState::Failed:       return "Failed";
        }
        return "?";
    }

    const char* gps_fix_name(int fix_type)
    {
        switch (fix_type) {
            case 0: return "No GPS";
            case 1: return "No fix";
            case 2: return "2D fix";
            case 3: return "3D fix";
            case 4: return "DGPS";
            case 5: return "RTK float";
            case 6: return "RTK fixed";
        }
        return "Unknown";
    }

#ifdef KAYRA_HAS_MAVSDK
    namespace {
        // Lifetime: Telemetry and System must be destroyed before Mavsdk.
        std::mutex g_link_mutex;
        std::unique_ptr<mavsdk::Mavsdk>    g_mavsdk;
        std::shared_ptr<mavsdk::System>    g_system;
        std::unique_ptr<mavsdk::Telemetry> g_telemetry;
        std::unique_ptr<mavsdk::MissionRaw> g_mission_raw;

        // Incremented on every connect/disconnect, so late callbacks from an old link are ignored.
        std::atomic<int> g_generation{0};

        std::atomic<bool>    g_mission_busy{false};         // a mission download/upload/clear is in flight
        std::atomic<int64_t> g_last_download_start{0};      // steady_clock ticks

        std::thread       g_worker;
        std::atomic<bool> g_cancel{false};

        constexpr double kDiscoveryTimeoutS = 10.0;

        // Asynchronous; safe to call from MAVSDK callbacks (takes no locks besides g_mission_mutex).
        void start_mission_download(mavsdk::MissionRaw& mission_raw, int generation)
        {
            set_mission_status("Downloading mission...");
            g_mission_busy = true;
            g_last_download_start = std::chrono::steady_clock::now().time_since_epoch().count();
            mission_raw.download_mission_async(
                [generation](mavsdk::MissionRaw::Result result, std::vector<mavsdk::MissionRaw::MissionItem> items) {
                    g_mission_busy = false;
                    if (generation != g_generation)
                        return;  // belongs to a link that has been closed
                    using R = mavsdk::MissionRaw::Result;
                    if (result == R::NoMissionAvailable) {
                        store_mission({}, "No mission on vehicle");
                        return;
                    }
                    if (result != R::Success) {
                        std::ostringstream ss;
                        ss << "Mission download failed: " << result;
                        set_mission_status(ss.str());
                        std::cout << "[backend] " << ss.str() << std::endl;
                        return;
                    }

                    std::vector<MissionItem> mission;
                    mission.reserve(items.size());
                    for (const auto& it : items) {
                        MissionItem m;
                        m.seq           = int(it.seq);
                        m.frame         = int(it.frame);
                        m.command       = int(it.command);
                        m.autocontinue  = int(it.autocontinue);
                        m.param1 = it.param1; m.param2 = it.param2; m.param3 = it.param3; m.param4 = it.param4;
                        m.latitude_deg  = it.x * 1e-7;
                        m.longitude_deg = it.y * 1e-7;
                        m.altitude_m    = it.z;
                        m.positional    = is_positional_command(m.command) && is_global_frame(m.frame)
                                       && (it.x != 0 || it.y != 0);
                        mission.push_back(m);
                    }
                    const std::string status = describe(mission);
                    std::cout << "[backend] Mission downloaded: " << status << std::endl;
                    store_mission(std::move(mission), status);
                });
        }

        void subscribe_mission(mavsdk::MissionRaw& mission_raw, int generation)
        {
            mission_raw.subscribe_mission_progress([&mission_raw, generation](const mavsdk::MissionRaw::MissionProgress& p) {
                Mission_current = p.current;
                if (generation != g_generation)
                    return;

                // The vehicle's mission differs from ours (e.g. uploaded after we connected):
                // its item count changed, or it reports progress while we hold no mission.
                const bool count_differs = p.total > 0 && p.total != Mission_total;
                const bool have_none     = Mission_total == 0 && p.current > 0;
                const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
                const auto min_gap = std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                                         std::chrono::seconds(5)).count();
                if ((count_differs || have_none) && !g_mission_busy && now - g_last_download_start > min_gap)
                    start_mission_download(mission_raw, generation);
            });
            // The mission on the vehicle was changed (e.g. uploaded by another GCS): fetch it again.
            mission_raw.subscribe_mission_changed([&mission_raw, generation](bool) {
                if (generation == g_generation)
                    start_mission_download(mission_raw, generation);
            });
            start_mission_download(mission_raw, generation);
        }

        void subscribe_all(mavsdk::Telemetry& telemetry, mavsdk::System& system)
        {
            using T = mavsdk::Telemetry;

            // MAVSDK 3 passes a Position, MAVSDK 4 a HomePosition; both have these fields.
            telemetry.subscribe_home([](const auto& home) {
                Home_latitude  = home.latitude_deg;
                Home_longitude = home.longitude_deg;
                Home_altitude_amsl = home.absolute_altitude_m;
            });

            telemetry.subscribe_position([](const T::Position& pos) {
                Position_latitude          = pos.latitude_deg;
                Position_longitude         = pos.longitude_deg;
                Position_altitude          = pos.absolute_altitude_m;
                Position_relative_altitude = pos.relative_altitude_m;
            });

            telemetry.subscribe_battery([](const T::Battery& batt) {
                Battery_temperature_deg   = batt.temperature_degc;
                Battery_voltage           = batt.voltage_v;
                Battery_current_battery   = batt.current_battery_a;
                Battery_capacity_consumed = batt.capacity_consumed_ah;
                Battery_remaining_percent = batt.remaining_percent;
                Battery_time_remaining    = batt.time_remaining_s;
            });

            telemetry.subscribe_attitude_euler([](const T::EulerAngle& e) {
                Attitude_roll_deg  = e.roll_deg;
                Attitude_pitch_deg = e.pitch_deg;
                Attitude_yaw_deg   = e.yaw_deg;
            });

            telemetry.subscribe_heading([](const T::Heading& h) {
                Heading_deg = static_cast<float>(h.heading_deg);
            });

            telemetry.subscribe_attitude_angular_velocity_body([](const T::AngularVelocityBody& a) {
                Angular_velocity_roll_rad  = a.roll_rad_s;
                Angular_velocity_pitch_rad = a.pitch_rad_s;
                Angular_velocity_yaw_rad   = a.yaw_rad_s;
            });

            telemetry.subscribe_fixedwing_metrics([](const T::FixedwingMetrics& m) {
                Airspeed_m_s     = m.airspeed_m_s;
                Groundspeed_m_s  = m.groundspeed_m_s;
                Climb_rate_m_s   = m.climb_rate_m_s;
                Throttle_percent = m.throttle_percentage;
            });

            telemetry.subscribe_velocity_ned([](const T::VelocityNed& v) {
                Velocity_x_m_s = v.north_m_s;
                Velocity_y_m_s = v.east_m_s;
                Velocity_z_m_s = v.down_m_s;
            });

            telemetry.subscribe_gps_info([](const T::GpsInfo& gps) {
                GPS_fix_type       = static_cast<int>(gps.fix_type);
                GPS_num_satellites = gps.num_satellites;
            });

            telemetry.subscribe_health([](const T::Health& h) {
                Is_gyrometer_calibration_ok     = h.is_gyrometer_calibration_ok;
                Is_accelerometer_calibration_ok = h.is_accelerometer_calibration_ok;
                Is_magnetometer_calibration_ok  = h.is_magnetometer_calibration_ok;
                Is_local_position_ok            = h.is_local_position_ok;
                Is_global_position_ok           = h.is_global_position_ok;
                Is_home_position_ok             = h.is_home_position_ok;
                Is_armable                      = h.is_armable;
            });

            telemetry.subscribe_armed([](bool armed) { Is_armed = armed; });

            telemetry.subscribe_flight_mode([](T::FlightMode mode) {
                std::ostringstream ss;
                ss << mode;
                std::lock_guard<std::mutex> lock(g_text_mutex);
                g_flight_mode = ss.str();
            });

            telemetry.subscribe_rc_status([](const T::RcStatus& rc) {
                Was_available_once      = rc.was_available_once;
                Is_available            = rc.is_available;
                Signal_strength_percent = rc.signal_strength_percent;
            });

            system.subscribe_is_connected([](bool connected) {
                const ConnectionState s = connection_state.load();
                if (!connected && s == ConnectionState::Connected) {
                    set_state(ConnectionState::LinkLost);
                    set_status("Link lost: no heartbeat from vehicle");
                }
                else if (connected && s == ConnectionState::LinkLost) {
                    set_state(ConnectionState::Connected);
                    set_status("Link restored");
                }
            });
        }

        void connect_worker(std::string url, int generation)
        {
            auto mav = std::make_unique<mavsdk::Mavsdk>(
                mavsdk::Mavsdk::Configuration{mavsdk::ComponentType::GroundStation});

            const mavsdk::ConnectionResult result = mav->add_any_connection(url);
            if (result != mavsdk::ConnectionResult::Success) {
                std::ostringstream ss;
                ss << "Connection failed: " << result;
                set_status(ss.str());
                set_state(ConnectionState::Failed);
                return;
            }

            set_status("Waiting for autopilot on " + url + " ...");

            // Poll in short slices so disconnect()/shutdown() can cancel quickly.
            std::optional<std::shared_ptr<mavsdk::System>> system;
            const auto deadline = std::chrono::steady_clock::now()
                                + std::chrono::duration<double>(kDiscoveryTimeoutS);
            while (!g_cancel && std::chrono::steady_clock::now() < deadline) {
                system = mav->first_autopilot(0.25);
                if (system)
                    break;
            }

            if (g_cancel) {
                set_state(ConnectionState::Disconnected);
                set_status("Connection cancelled");
                return;
            }
            if (!system) {
                set_state(ConnectionState::Failed);
                set_status("No autopilot found within 10 s on " + url);
                return;
            }

            auto telemetry = std::make_unique<mavsdk::Telemetry>(*system);
            subscribe_all(*telemetry, **system);
            auto mission_raw = std::make_unique<mavsdk::MissionRaw>(*system);
            subscribe_mission(*mission_raw, generation);

            {
                std::lock_guard<std::mutex> lock(g_link_mutex);
                g_mavsdk      = std::move(mav);
                g_system      = *system;
                g_telemetry   = std::move(telemetry);
                g_mission_raw = std::move(mission_raw);
            }
            set_state(ConnectionState::Connected);
            set_status("Connected to autopilot on " + url);
        }

        void join_worker()
        {
            if (g_worker.joinable())
                g_worker.join();
        }

        // Joins the worker if main() forgets to call shutdown() (an unjoined std::thread aborts).
        struct ShutdownGuard { ~ShutdownGuard() { shutdown(); } } g_shutdown_guard;
    }

    bool has_mavsdk() { return true; }

    bool connect(const std::string& connection_url)
    {
        const ConnectionState s = connection_state.load();
        if (s == ConnectionState::Connecting || s == ConnectionState::Connected || s == ConnectionState::LinkLost)
            return false;

        join_worker();
        reset_values();
        g_cancel = false;
        set_state(ConnectionState::Connecting);
        set_status("Connecting to " + connection_url + " ...");
        g_worker = std::thread(connect_worker, connection_url, ++g_generation);
        return true;
    }

    void disconnect()
    {
        g_cancel = true;
        join_worker();
        ++g_generation;
        {
            std::lock_guard<std::mutex> lock(g_link_mutex);
            g_mission_raw.reset();
            g_telemetry.reset();
            g_system.reset();
            g_mavsdk.reset();
        }
        g_cancel = false;
        if (connection_state != ConnectionState::Disconnected) {
            set_state(ConnectionState::Disconnected);
            set_status("Disconnected");
        }
        reset_values();
    }

    void shutdown()
    {
        disconnect();
    }

    bool mission_busy() { return g_mission_busy; }

    void reload_mission()
    {
        std::lock_guard<std::mutex> lock(g_link_mutex);
        if (g_mission_raw && !g_mission_busy)
            start_mission_download(*g_mission_raw, g_generation);
    }

    bool upload_mission(std::vector<MissionItem> items)
    {
        std::lock_guard<std::mutex> lock(g_link_mutex);
        if (!g_mission_raw || g_mission_busy)
            return false;
        if (items.empty())
            return false;  // use clear_vehicle_mission()

        std::vector<mavsdk::MissionRaw::MissionItem> raw;
        raw.reserve(items.size());
        for (size_t i = 0; i < items.size(); ++i) {
            MissionItem& m = items[i];
            m.seq = int(i);
            m.positional = is_positional_command(m.command);
            if (m.positional && !is_global_frame(m.frame))
                m.frame = 6;
            if (!m.positional && m.command == 20)  // RETURN_TO_LAUNCH has no position
                m.frame = 2;

            mavsdk::MissionRaw::MissionItem r{};
            r.seq          = uint32_t(i);
            r.frame        = uint32_t(m.frame);
            r.command      = uint32_t(m.command);
            r.current      = i == 0 ? 1 : 0;
            r.autocontinue = uint32_t(m.autocontinue);
            r.param1 = m.param1; r.param2 = m.param2; r.param3 = m.param3; r.param4 = m.param4;
            r.x = m.positional ? int32_t(std::lround(m.latitude_deg * 1e7))  : 0;
            r.y = m.positional ? int32_t(std::lround(m.longitude_deg * 1e7)) : 0;
            r.z = m.positional ? m.altitude_m : 0.0f;
            r.mission_type = 0;  // MAV_MISSION_TYPE_MISSION
            raw.push_back(r);
        }

        g_mission_busy = true;
        set_mission_status("Uploading " + std::to_string(raw.size()) + " items...");
        const int generation = g_generation;
        g_mission_raw->upload_mission_async(raw, [generation, items](mavsdk::MissionRaw::Result result) {
            g_mission_busy = false;
            if (generation != g_generation)
                return;
            std::ostringstream ss;
            if (result == mavsdk::MissionRaw::Result::Success) {
                ss << "Uploaded: " << describe(items);
                std::cout << "[backend] " << ss.str() << std::endl;
                Mission_current = -1;  // the vehicle restarts at item 0 and reports it
                store_mission(items, ss.str());
            }
            else {
                ss << "Upload failed: " << result;
                std::cout << "[backend] " << ss.str() << std::endl;
                set_mission_status(ss.str());
            }
        });
        return true;
    }

    bool clear_vehicle_mission()
    {
        std::lock_guard<std::mutex> lock(g_link_mutex);
        if (!g_mission_raw || g_mission_busy)
            return false;
        g_mission_busy = true;
        set_mission_status("Clearing vehicle mission...");
        const int generation = g_generation;
        g_mission_raw->clear_mission_async([generation](mavsdk::MissionRaw::Result result) {
            g_mission_busy = false;
            if (generation != g_generation)
                return;
            if (result == mavsdk::MissionRaw::Result::Success) {
                Mission_current = -1;
                store_mission({}, "Vehicle mission cleared");
            }
            else {
                std::ostringstream ss;
                ss << "Clear failed: " << result;
                set_mission_status(ss.str());
            }
        });
        return true;
    }

#else  // ---- Built without MAVSDK: keep the GUI working, report why it can't connect ----

    bool has_mavsdk() { return false; }

    bool connect(const std::string&)
    {
        set_state(ConnectionState::Failed);
        set_status("KAYRA was built without MAVSDK; install it and rebuild to connect");
        return false;
    }

    void disconnect()
    {
        set_state(ConnectionState::Disconnected);
        reset_values();
    }

    void shutdown() {}

    bool mission_busy() { return false; }
    void reload_mission() {}
    bool upload_mission(std::vector<MissionItem>) { return false; }
    bool clear_vehicle_mission() { return false; }

#endif
}
