#pragma once

// Mission export as JSON in QGroundControl's .plan format (fileType "Plan", version 1),
// so exported missions open in QGroundControl and in MAVSDK's import_qgroundcontrol_mission().

#include "backend.h"

#include <string>
#include <vector>

namespace MissionExport {
    struct PlanInfo {
        double home_lat = 0.0, home_lon = 0.0;  // planned home position
        double home_alt_amsl = 0.0;
        float  cruise_speed_m_s = 10.0f;
    };

    std::string ToPlanJson(const std::vector<backend::MissionItem>& items, const PlanInfo& info);

    // Writes the file; on failure returns false and sets `error`.
    bool WriteFile(const std::string& path, const std::string& contents, std::string& error);
}
