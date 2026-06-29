#include "backend.h"


namespace backend{
    std::atomic<float> relative_altitude= 0.0f; 
    std::atomic<float> Altitude_Value = 0.0f;


    std::atomic<float> Battery_temperature_deg = 0.0f;
    std::atomic<float> Battery_voltage = 0.0f;
    std::atomic<float> Battery_current_battery = 0.0f;
    std::atomic<float> Battery_capacity_consumed = 0.0f;
    std::atomic<float> Battery_remaining_percent = 0.0f;
    std::atomic<float> Battery_time_remaining = 0.0f;

    std::atomic<double>Position_latitude = 0.0;
    std::atomic<double>Position_longitude = 0.0;
    std::atomic<float> Position_altitude = 0.0f;
    std::atomic<float> Position_relative_altitude = 0.0f;

    std::atomic<float> Angular_velocity_roll_rad = 0.0f;
    std::atomic<float> Angular_velocity_pitch_rad = 0.0f;
    std::atomic<float> Angular_velocity_yaw_rad = 0.0f;

    std::atomic<int >  GPS_fix_type = 0;

    std::atomic<bool > Is_gyrometer_calibration_ok = false;
    std::atomic<bool > Is_accelerometer_calibration_ok = false;
    std::atomic<bool > Is_magnetometer_calibration_ok = false;
    std::atomic<bool > Is_local_position_ok = false;
    std::atomic<bool > Is_global_position_ok = false;
    std::atomic<bool > Is_home_position_ok = false;
    std::atomic<bool > Is_armable = false;
    std::atomic<bool > Is_armed = false;

    std::atomic<float> Velocity_x_m_s = 0.0f;
    std::atomic<float> Velocity_y_m_s = 0.0f;
    std::atomic<float> Velocity_z_m_s = 0.0f;

    std::atomic<bool > Was_available_once = false;
    std::atomic<bool > Is_available = false;
    std::atomic<int >  Signal_strength_percent = false;

    bool mavsdk_connect_requested = false;
    bool mavsdk_connected = false;  
    
    std::unique_ptr<mavsdk::Mavsdk> global_mavsdk = nullptr ;
    std::shared_ptr<mavsdk::System> global_system  = nullptr ;



    void mavsdk_connection(char* mavsdk_connection_str){
        global_mavsdk = std::make_unique<mavsdk::Mavsdk>(
            mavsdk::Mavsdk::Configuration{mavsdk::ComponentType::GroundStation}
        );
        mavsdk::ConnectionResult connection_result = global_mavsdk->add_any_connection(mavsdk_connection_str);


        if (connection_result != mavsdk::ConnectionResult::Success) {
            std::cerr << "Connection failed: " << connection_result << std::endl;
            mavsdk_connect_requested = false;
            mavsdk_connected = false;
            global_mavsdk.reset();
        } 
        else {
            std::cout << "Connection successful." << std::endl;
        } 


        //std::shared_ptr<mavsdk::System> global_system = nullptr;
        //auto system = mavsdk.first_autopilot(3.0);
        auto system = global_mavsdk->first_autopilot(3.0);
        if (!system) {
            std::cerr << "Timed out waiting for system\n";
            global_mavsdk.reset();
        }
        global_system = system.value();

        start_telemetry_thread(global_system);
    }



void start_telemetry_thread(std::shared_ptr<mavsdk::System> system)
{
    std::thread([system]() {

        mavsdk::Telemetry telemetry(system);

        telemetry.subscribe_position(
            [](const mavsdk::Telemetry::Position& pos)
            {
                relative_altitude = pos.relative_altitude_m;

                Position_latitude = pos.latitude_deg;
                Position_longitude = pos.longitude_deg;
                Position_altitude = pos.absolute_altitude_m;
                Position_relative_altitude = pos.relative_altitude_m;
            });

        telemetry.subscribe_battery(
            [](const mavsdk::Telemetry::Battery& batt)
            {
                Battery_temperature_deg = batt.temperature_degc;
                Battery_voltage = batt.voltage_v;
                Battery_current_battery = batt.current_battery_a;
                Battery_capacity_consumed = batt.capacity_consumed_ah;
                Battery_remaining_percent = batt.remaining_percent;
                Battery_time_remaining = batt.time_remaining_s;
            });

        telemetry.subscribe_attitude_angular_velocity_body(
            [](const mavsdk::Telemetry::AngularVelocityBody& angular)
            {
                Angular_velocity_roll_rad  = angular.roll_rad_s;
                Angular_velocity_pitch_rad = angular.pitch_rad_s;
                Angular_velocity_yaw_rad   = angular.yaw_rad_s;
            });

        telemetry.subscribe_gps_info(
            [](const mavsdk::Telemetry::GpsInfo& gps)
            {
                GPS_fix_type = static_cast<int>(gps.fix_type);
            });

        telemetry.subscribe_health(
            [](const mavsdk::Telemetry::Health& health)
            {
                Is_gyrometer_calibration_ok     = health.is_gyrometer_calibration_ok;
                Is_accelerometer_calibration_ok = health.is_accelerometer_calibration_ok;
                Is_magnetometer_calibration_ok  = health.is_magnetometer_calibration_ok;

                Is_local_position_ok  = health.is_local_position_ok;
                Is_global_position_ok = health.is_global_position_ok;
                Is_home_position_ok   = health.is_home_position_ok;

                Is_armable = health.is_armable;
            });

        telemetry.subscribe_armed(
            [](bool armed)
            {
                Is_armed = armed;
            });

        telemetry.subscribe_velocity_ned(
            [](const mavsdk::Telemetry::VelocityNed& vel)
            {
                Velocity_x_m_s = vel.north_m_s;
                Velocity_y_m_s = vel.east_m_s;
                Velocity_z_m_s = vel.down_m_s;
            });

        telemetry.subscribe_rc_status(
            [](const mavsdk::Telemetry::RcStatus& rc)
            {
                Was_available_once      = rc.was_available_once;
                Is_available            = rc.is_available;
                Signal_strength_percent = rc.signal_strength_percent;
            });

        while (true)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

    }).detach();
}
}