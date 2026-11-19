// kayra-sim: a minimal fake vehicle for testing KAYRA without PX4/ArduPilot.
//
// It is a real MAVLink autopilot (built on MAVSDK's server plugins) that
//   - holds a mission on its MissionRawServer, which KAYRA downloads like from a real vehicle,
//   - flies that mission at constant speed, advancing MISSION_CURRENT at each waypoint,
//   - publishes position, home, heading, attitude, speeds, GPS and battery.
//
// Usage:  kayra-sim [udpout://127.0.0.1:14540] [home_lat home_lon]
// Then in KAYRA connect to the default udpin://0.0.0.0:14540 (same port as PX4 SITL).

#if __has_include(<mavsdk/mavsdk.hpp>)
#include <mavsdk/mavsdk.hpp>
#include <mavsdk/plugins/mission_raw/mission_raw.hpp>
#include <mavsdk/plugins/mission_raw_server/mission_raw_server.hpp>
#include <mavsdk/plugins/telemetry_server/telemetry_server.hpp>
#else
#include <mavsdk/mavsdk.h>
#include <mavsdk/plugins/mission_raw/mission_raw.h>
#include <mavsdk/plugins/mission_raw_server/mission_raw_server.h>
#include <mavsdk/plugins/telemetry_server/telemetry_server.h>
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace mavsdk;
using namespace std::chrono_literals;

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthRadiusM = 6371000.0;
constexpr double kCruiseSpeed = 12.0;   // m/s
constexpr double kClimbSpeed = 3.0;     // m/s
constexpr double kAcceptRadius = 4.0;   // m, waypoint reached
constexpr double kRateHz = 20.0;

std::atomic<bool> g_stop{false};

// Local east/north offset in metres -> lat/lon.
void offset(double lat, double lon, double east_m, double north_m, double& out_lat, double& out_lon)
{
    out_lat = lat + north_m / kEarthRadiusM * 180.0 / kPi;
    out_lon = lon + east_m / (kEarthRadiusM * std::cos(lat * kPi / 180.0)) * 180.0 / kPi;
}

MissionRaw::MissionItem make_item(uint32_t seq, uint32_t command, double lat, double lon, float rel_alt)
{
    MissionRaw::MissionItem it{};
    it.seq = seq;
    it.frame = 6;  // MAV_FRAME_GLOBAL_RELATIVE_ALT_INT
    it.command = command;
    it.current = seq == 0 ? 1 : 0;
    it.autocontinue = 1;
    it.x = int32_t(std::lround(lat * 1e7));
    it.y = int32_t(std::lround(lon * 1e7));
    it.z = rel_alt;
    it.mission_type = 0;
    return it;
}

// Takeoff, a loop around home with a speed-change command in the middle (a non-positional
// item, to exercise KAYRA's "next waypoint" logic), then land at home.
std::vector<MissionRaw::MissionItem> demo_mission(double home_lat, double home_lon)
{
    const double pts[][3] = {  // east m, north m, rel alt m
        {0, 0, 20}, {150, 120, 40}, {320, 60, 50}, {380, -140, 50}, {180, -260, 45}, {-80, -180, 40}, {-140, 40, 30},
    };
    std::vector<MissionRaw::MissionItem> items;
    uint32_t seq = 0;
    for (size_t i = 0; i < sizeof(pts) / sizeof(pts[0]); ++i) {
        double lat, lon;
        offset(home_lat, home_lon, pts[i][0], pts[i][1], lat, lon);
        items.push_back(make_item(seq++, i == 0 ? 22 /* TAKEOFF */ : 16 /* WAYPOINT */, lat, lon, float(pts[i][2])));
        if (i == 3) {
            auto speed = make_item(seq++, 178 /* DO_CHANGE_SPEED */, 0, 0, 0);
            speed.frame = 2;  // MAV_FRAME_MISSION
            speed.param2 = 12.0f;
            items.push_back(speed);
        }
    }
    items.push_back(make_item(seq++, 21 /* LAND */, home_lat, home_lon, 0));
    return items;
}

double bearing_deg(double lat1, double lon1, double lat2, double lon2)
{
    const double p1 = lat1 * kPi / 180.0, p2 = lat2 * kPi / 180.0, dl = (lon2 - lon1) * kPi / 180.0;
    return std::fmod(std::atan2(std::sin(dl) * std::cos(p2),
                                std::cos(p1) * std::sin(p2) - std::sin(p1) * std::cos(p2) * std::cos(dl)) * 180.0 / kPi + 360.0, 360.0);
}

double distance_m(double lat1, double lon1, double lat2, double lon2)
{
    const double p1 = lat1 * kPi / 180.0, p2 = lat2 * kPi / 180.0;
    const double dp = p2 - p1, dl = (lon2 - lon1) * kPi / 180.0;
    const double a = std::sin(dp / 2) * std::sin(dp / 2) + std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
    return 2.0 * kEarthRadiusM * std::asin(std::min(1.0, std::sqrt(a)));
}

// Uploads the demo mission to our own autopilot over a private local link, exactly as a GCS would.
// Port of the private link used to load the demo mission: main port + 59 (14540 -> 14599),
// so several simulators on different ports don't collide.
int internal_port(const std::string& url)
{
    const size_t colon = url.rfind(':');
    const int port = colon == std::string::npos ? 0 : std::atoi(url.c_str() + colon + 1);
    return (port > 0 ? port : 14540) + 59;
}

bool upload_mission(const std::vector<MissionRaw::MissionItem>& items, int port)
{
    Mavsdk gcs{Mavsdk::Configuration{246, 190, true}};  // sysid differs from KAYRA's
    if (gcs.add_any_connection("udpout://127.0.0.1:" + std::to_string(port)) != ConnectionResult::Success)
        return false;
    auto system = gcs.first_autopilot(10.0);
    if (!system)
        return false;
    MissionRaw mission{*system};
    const auto result = mission.upload_mission(items);
    if (result != MissionRaw::Result::Success) {
        std::cerr << "kayra-sim: mission upload failed: " << result << "\n";
        return false;
    }
    return true;
}
} // namespace

int main(int argc, char** argv)
{
    std::string url = "udpout://127.0.0.1:14540";
    double home_lat = 47.397742, home_lon = 8.545594;  // PX4 SITL default home (Zurich)
    if (argc >= 2) url = argv[1];
    if (argc >= 4) { home_lat = std::atof(argv[2]); home_lon = std::atof(argv[3]); }

    std::cout << std::unitbuf;  // log lines appear immediately, also when redirected to a file
    std::signal(SIGINT, [](int) { g_stop = true; });
    std::signal(SIGTERM, [](int) { g_stop = true; });

    Mavsdk mavsdk{Mavsdk::Configuration{ComponentType::Autopilot}};
    if (mavsdk.add_any_connection(url) != ConnectionResult::Success ||
        mavsdk.add_any_connection("udpin://127.0.0.1:" + std::to_string(internal_port(url))) != ConnectionResult::Success) {
        std::cerr << "kayra-sim: could not open " << url << "\n";
        return 1;
    }

    auto server = mavsdk.server_component();
    TelemetryServer telemetry{server};
    MissionRawServer mission_server{server};

    std::mutex mutex;
    std::vector<MissionRaw::MissionItem> mission;
    int current = -1;

    mission_server.subscribe_incoming_mission([&](MissionRawServer::Result result, MissionRawServer::MissionPlan plan) {
        if (result != MissionRawServer::Result::Success)
            return;
        std::lock_guard<std::mutex> lock(mutex);
        mission.clear();
        for (const auto& it : plan.mission_items) {
            MissionRaw::MissionItem m{};
            m.seq = it.seq; m.frame = it.frame; m.command = it.command; m.x = it.x; m.y = it.y; m.z = it.z;
            mission.push_back(m);
        }
        std::cout << "kayra-sim: mission received, " << mission.size() << " items\n";
    });
    mission_server.subscribe_current_item_changed([&](MissionRawServer::MissionItem item) {
        std::lock_guard<std::mutex> lock(mutex);
        current = int(item.seq);
        std::cout << "kayra-sim: flying to mission item " << current << "\n";
    });

    std::cout << "kayra-sim: autopilot on " << url << ", home " << home_lat << ", " << home_lon << "\n";
    std::thread uploader([&] {
        if (!upload_mission(demo_mission(home_lat, home_lon), internal_port(url)))
            std::cerr << "kayra-sim: could not load the demo mission\n";
    });

    // Vehicle state
    double lat = home_lat, lon = home_lon, alt = 0.0, heading = 0.0, roll = 0.0;
    double battery = 100.0;
    const double home_amsl = 488.0;
    const double dt = 1.0 / kRateHz;
    auto next_tick = std::chrono::steady_clock::now();
    int tick = 0;
    double hold_s = 0.0;  // wait a few seconds on the ground before taking off

    while (!g_stop) {
        double vn = 0, ve = 0, vd = 0, target_heading = heading;
        bool advance = false;
        {
            std::lock_guard<std::mutex> lock(mutex);
            hold_s += dt;
            if (hold_s > 8.0 && current >= 0 && current < int(mission.size())) {
                const auto& it = mission[current];
                const bool positional = it.frame != 2 && (it.x != 0 || it.y != 0);
                if (!positional) {
                    advance = true;  // commands complete immediately
                }
                else {
                    const double tlat = it.x * 1e-7, tlon = it.y * 1e-7;
                    const double d = distance_m(lat, lon, tlat, tlon);
                    const double dz = it.z - alt;
                    if (d < kAcceptRadius && std::abs(dz) < 1.0) {
                        advance = true;
                    }
                    else {
                        if (d > 0.5) {
                            target_heading = bearing_deg(lat, lon, tlat, tlon);
                            const double step = std::min(kCruiseSpeed, d / dt);
                            vn = step * std::cos(target_heading * kPi / 180.0);
                            ve = step * std::sin(target_heading * kPi / 180.0);
                        }
                        vd = -std::clamp(dz, -kClimbSpeed, kClimbSpeed);
                    }
                }
            }
        }
        if (advance)
            mission_server.set_current_item_complete();

        // Integrate
        offset(lat, lon, ve * dt, vn * dt, lat, lon);
        alt = std::max(0.0, alt - vd * dt);
        double dh = std::fmod(target_heading - heading + 540.0, 360.0) - 180.0;
        const double max_turn = 45.0 * dt;  // deg per tick
        dh = std::clamp(dh, -max_turn, max_turn);
        heading = std::fmod(heading + dh + 360.0, 360.0);
        roll += (std::clamp(dh / max_turn * 25.0, -25.0, 25.0) - roll) * 0.1;
        const double ground_speed = std::sqrt(vn * vn + ve * ve);
        if (ground_speed > 0.1 || alt > 0.5)
            battery = std::max(0.0, battery - 0.004);

        // Publish
        TelemetryServer::Position pos{};
        pos.latitude_deg = lat;
        pos.longitude_deg = lon;
        pos.absolute_altitude_m = float(home_amsl + alt);
        pos.relative_altitude_m = float(alt);
        TelemetryServer::VelocityNed vel{};
        vel.north_m_s = float(vn); vel.east_m_s = float(ve); vel.down_m_s = float(vd);
        TelemetryServer::Heading hdg{};
        hdg.heading_deg = heading;
        telemetry.publish_position(pos, vel, hdg);

        TelemetryServer::EulerAngle att{};
        att.roll_deg = float(roll);
        att.pitch_deg = float(ground_speed > 0.1 ? -4.0 : 0.0);
        att.yaw_deg = float(heading > 180.0 ? heading - 360.0 : heading);
        TelemetryServer::AngularVelocityBody rates{};
        rates.roll_rad_s = 0; rates.pitch_rad_s = 0; rates.yaw_rad_s = float(dh / dt * kPi / 180.0);
        telemetry.publish_attitude(att, rates);

        TelemetryServer::FixedwingMetrics hud{};
        hud.airspeed_m_s = float(ground_speed);
        hud.groundspeed_m_s = float(ground_speed);
        hud.climb_rate_m_s = float(-vd);
        hud.throttle_percentage = float(ground_speed > 0.1 || vd != 0 ? 55.0 : 0.0);
        hud.heading_deg = float(heading);
        hud.absolute_altitude_m = float(home_amsl + alt);
        telemetry.publish_visual_flight_rules_hud(hud);

        if (tick % int(kRateHz) == 0) {  // 1 Hz
            TelemetryServer::Position home{};
            home.latitude_deg = home_lat;
            home.longitude_deg = home_lon;
            home.absolute_altitude_m = float(home_amsl);
            home.relative_altitude_m = 0.0f;
            telemetry.publish_home(home);

            TelemetryServer::RawGps raw{};
            raw.latitude_deg = lat; raw.longitude_deg = lon; raw.absolute_altitude_m = float(home_amsl + alt);
            raw.hdop = 0.8f; raw.vdop = 1.1f; raw.velocity_m_s = float(ground_speed); raw.cog_deg = float(heading);
            TelemetryServer::GpsInfo gps{};
            gps.num_satellites = 14;
            gps.fix_type = TelemetryServer::FixType::Fix3D;
            telemetry.publish_raw_gps(raw, gps);

            TelemetryServer::Battery batt{};
            batt.voltage_v = float(14.8 + 2.0 * battery / 100.0);
            batt.remaining_percent = float(battery / 100.0);  // server side expects 0..1 (client reports 0..100)
            telemetry.publish_sys_status(batt, true, true, true, true, true);
            telemetry.publish_battery(batt);
        }
        ++tick;

        next_tick += std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(dt));
        std::this_thread::sleep_until(next_tick);
    }

    uploader.join();
    std::cout << "kayra-sim: stopped\n";
    return 0;
}
