#include "Mission_Export.h"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace MissionExport {
    namespace {
        // JSON number; NaN/inf become null (QGC uses null for "unset", e.g. yaw).
        std::string Num(double v, int decimals = 6)
        {
            if (!std::isfinite(v))
                return "null";
            if (v == std::floor(v) && std::fabs(v) < 1e15) {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.0f", v);
                return buf;
            }
            char buf[48];
            snprintf(buf, sizeof(buf), "%.*f", decimals, v);
            // Trim trailing zeros: 12.500000 -> 12.5
            std::string s = buf;
            while (!s.empty() && s.back() == '0') s.pop_back();
            if (!s.empty() && s.back() == '.') s.pop_back();
            return s;
        }

        // QGC writes the float frames: GLOBAL(_INT) -> 0, RELATIVE_ALT(_INT) -> 3, TERRAIN_ALT(_INT) -> 10.
        int PlanFrame(int frame)
        {
            switch (frame) {
                case 5:  return 0;
                case 6:  return 3;
                case 11: return 10;
            }
            return frame;
        }

        // QGC AltitudeMode: 1 relative to home, 2 AMSL, 3 above terrain.
        int AltitudeMode(int plan_frame)
        {
            if (plan_frame == 0)  return 2;
            if (plan_frame == 10) return 3;
            return 1;
        }
    }

    std::string ToPlanJson(const std::vector<backend::MissionItem>& items, const PlanInfo& info)
    {
        std::ostringstream o;
        o << "{\n"
          << "    \"fileType\": \"Plan\",\n"
          << "    \"geoFence\": {\n"
          << "        \"circles\": [],\n"
          << "        \"polygons\": [],\n"
          << "        \"version\": 2\n"
          << "    },\n"
          << "    \"groundStation\": \"KAYRA\",\n"
          << "    \"mission\": {\n"
          << "        \"cruiseSpeed\": " << Num(info.cruise_speed_m_s, 2) << ",\n"
          << "        \"firmwareType\": 0,\n"
          << "        \"hoverSpeed\": " << Num(info.cruise_speed_m_s, 2) << ",\n"
          << "        \"items\": [";

        for (size_t i = 0; i < items.size(); ++i) {
            const backend::MissionItem& it = items[i];
            const int frame = PlanFrame(it.positional ? it.frame : (it.frame == 6 ? 2 : it.frame));
            const double lat = it.positional ? it.latitude_deg : 0.0;
            const double lon = it.positional ? it.longitude_deg : 0.0;
            const double alt = it.positional ? it.altitude_m : 0.0;

            o << (i ? ",\n" : "\n") << "            {\n";
            if (it.positional) {
                o << "                \"AMSLAltAboveTerrain\": null,\n"
                  << "                \"Altitude\": " << Num(alt, 2) << ",\n"
                  << "                \"AltitudeMode\": " << AltitudeMode(frame) << ",\n";
            }
            o << "                \"autoContinue\": " << (it.autocontinue ? "true" : "false") << ",\n"
              << "                \"command\": " << it.command << ",\n"
              << "                \"doJumpId\": " << (i + 1) << ",\n"
              << "                \"frame\": " << frame << ",\n"
              << "                \"params\": [\n"
              << "                    " << Num(it.param1, 4) << ",\n"
              << "                    " << Num(it.param2, 4) << ",\n"
              << "                    " << Num(it.param3, 4) << ",\n"
              << "                    " << Num(it.param4, 4) << ",\n"
              << "                    " << Num(lat, 8) << ",\n"
              << "                    " << Num(lon, 8) << ",\n"
              << "                    " << Num(alt, 2) << "\n"
              << "                ],\n"
              << "                \"type\": \"SimpleItem\"\n"
              << "            }";
        }
        o << (items.empty() ? "],\n" : "\n        ],\n")
          << "        \"plannedHomePosition\": [\n"
          << "            " << Num(info.home_lat, 8) << ",\n"
          << "            " << Num(info.home_lon, 8) << ",\n"
          << "            " << Num(info.home_alt_amsl, 2) << "\n"
          << "        ],\n"
          << "        \"vehicleType\": 0,\n"
          << "        \"version\": 2\n"
          << "    },\n"
          << "    \"rallyPoints\": {\n"
          << "        \"points\": [],\n"
          << "        \"version\": 2\n"
          << "    },\n"
          << "    \"version\": 1\n"
          << "}\n";
        return o.str();
    }

    bool WriteFile(const std::string& path, const std::string& contents, std::string& error)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = std::string("Could not open file: ") + std::strerror(errno);
            return false;
        }
        out << contents;
        out.close();
        if (!out) {
            error = "Writing the file failed";
            return false;
        }
        return true;
    }
}
