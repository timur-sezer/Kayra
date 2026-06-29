#ifndef BACKEND_H
#define BACKEND_H

#include <chrono>
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/action/action.h>
#include <mavsdk/plugins/telemetry/telemetry.h>

namespace backend {
    extern bool mavsdk_connect_requested;
    extern bool mavsdk_connected;
    extern std::atomic<float> relative_altitude;
    extern std::atomic<float> Altitude_Value;

    
    extern std::atomic<float> Battery_temperature_deg;
    extern std::atomic<float> Battery_voltage;
    extern std::atomic<float> Battery_current_battery;
    extern std::atomic<float> Battery_capacity_consumed;
    extern std::atomic<float> Battery_remaining_percent;
    extern std::atomic<float> Battery_time_remaining;

    extern std::atomic<double>Position_latitude;
    extern std::atomic<double>Position_longitude;
    extern std::atomic<float> Position_altitude;
    extern std::atomic<float> Position_relative_altitude;

    extern std::atomic<float> Angular_velocity_roll_rad;
    extern std::atomic<float> Angular_velocity_pitch_rad;
    extern std::atomic<float> Angular_velocity_yaw_rad;

    extern std::atomic<int >  GPS_fix_type;

    extern std::atomic<bool > Is_gyrometer_calibration_ok;
    extern std::atomic<bool > Is_accelerometer_calibration_ok;
    extern std::atomic<bool > Is_magnetometer_calibration_ok;
    extern std::atomic<bool > Is_local_position_ok;
    extern std::atomic<bool > Is_global_position_ok;
    extern std::atomic<bool > Is_home_position_ok;
    extern std::atomic<bool > Is_armable;
    extern std::atomic<bool > Is_armed;

    extern std::atomic<float> Velocity_x_m_s;
    extern std::atomic<float> Velocity_y_m_s;
    extern std::atomic<float> Velocity_z_m_s;

    extern std::atomic<bool > Was_available_once;
    extern std::atomic<bool > Is_available;
    extern std::atomic<int >  Signal_strength_percent;





    extern std::unique_ptr<mavsdk::Mavsdk> global_mavsdk ;
    extern std::shared_ptr<mavsdk::System> global_system ;

    void mavsdk_connection(char* mavsdk_connection_str);
    void start_telemetry_thread(std::shared_ptr<mavsdk::System> system);
};


#endif
