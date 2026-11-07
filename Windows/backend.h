#ifndef BACKEND_H
#define BACKEND_H

// Telemetry backend (MAVSDK).
//
// This header deliberately does not include MAVSDK, so the GUI can include it freely.
// Every value below is written by MAVSDK's callback thread and read by the GUI thread,
// so they are all std::atomic. A float/double value of NaN means "no data yet".

#include <atomic>
#include <string>

namespace backend {
    enum class ConnectionState : int {
        Disconnected = 0,
        Connecting,      // waiting for an autopilot heartbeat
        Connected,
        LinkLost,        // was connected, heartbeats stopped
        Failed,          // connect() failed; see status_text()
    };

    extern std::atomic<ConnectionState> connection_state;
    extern std::atomic<bool> mavsdk_connected;          // true while ConnectionState::Connected

    // --- Battery ---
    extern std::atomic<float> Battery_temperature_deg;
    extern std::atomic<float> Battery_voltage;
    extern std::atomic<float> Battery_current_battery;
    extern std::atomic<float> Battery_capacity_consumed;
    extern std::atomic<float> Battery_remaining_percent; // 0..100
    extern std::atomic<float> Battery_time_remaining;    // seconds

    // --- Position ---
    extern std::atomic<double> Position_latitude;
    extern std::atomic<double> Position_longitude;
    extern std::atomic<float>  Position_altitude;          // AMSL, m
    extern std::atomic<float>  Position_relative_altitude; // above takeoff, m

    // --- Attitude ---
    extern std::atomic<float> Attitude_roll_deg;   // right wing down positive
    extern std::atomic<float> Attitude_pitch_deg;  // nose up positive
    extern std::atomic<float> Attitude_yaw_deg;
    extern std::atomic<float> Heading_deg;         // 0..360, 0 = north

    extern std::atomic<float> Angular_velocity_roll_rad;   // rad/s
    extern std::atomic<float> Angular_velocity_pitch_rad;
    extern std::atomic<float> Angular_velocity_yaw_rad;

    // --- Speeds ---
    extern std::atomic<float> Velocity_x_m_s;   // north
    extern std::atomic<float> Velocity_y_m_s;   // east
    extern std::atomic<float> Velocity_z_m_s;   // down
    extern std::atomic<float> Airspeed_m_s;
    extern std::atomic<float> Groundspeed_m_s;
    extern std::atomic<float> Climb_rate_m_s;   // up positive
    extern std::atomic<float> Throttle_percent;

    // --- GPS ---
    extern std::atomic<int> GPS_fix_type;       // mavsdk::Telemetry::FixType as int
    extern std::atomic<int> GPS_num_satellites;

    // --- Health ---
    extern std::atomic<bool> Is_gyrometer_calibration_ok;
    extern std::atomic<bool> Is_accelerometer_calibration_ok;
    extern std::atomic<bool> Is_magnetometer_calibration_ok;
    extern std::atomic<bool> Is_local_position_ok;
    extern std::atomic<bool> Is_global_position_ok;
    extern std::atomic<bool> Is_home_position_ok;
    extern std::atomic<bool> Is_armable;
    extern std::atomic<bool> Is_armed;

    // --- RC link ---
    extern std::atomic<bool>  Was_available_once;
    extern std::atomic<bool>  Is_available;
    extern std::atomic<float> Signal_strength_percent;  // 0..100, NaN if unknown

    // Starts connecting in a background thread; returns immediately.
    // Examples: "udpin://0.0.0.0:14540" (PX4 SITL), "serial:///dev/ttyUSB0:57600".
    // Returns false if already connecting/connected.
    bool connect(const std::string& connection_url);

    // Stops telemetry and closes the connection. Safe to call at any time.
    void disconnect();

    // Call once before the program exits.
    void shutdown();

    std::string status_text();   // human-readable connection status / last error
    std::string flight_mode();   // e.g. "Hold", "Mission"; empty if unknown

    const char* connection_state_name(ConnectionState state);
    const char* gps_fix_name(int fix_type);

    bool has_mavsdk();           // false if KAYRA was built without MAVSDK
}

#endif
