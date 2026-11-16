#include "Mission_Plan.h"

#include "File_Dialog.h"
#include "Mission_Export.h"
#include "Settings.h"
#include "backend.h"
#include "MapWidget.h"
#include "imgui.h"
#include "imgui_internal.h"  // DockBuilder API
#include "imgui_node_editor.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ed = ax::NodeEditor;

namespace MissionPlan {
    namespace {
        constexpr double kPi = 3.14159265358979323846;
        constexpr double kEarthRadiusM = 6371000.0;

        // MAV_CMD values used by the editor
        constexpr int kCmdWaypoint   = 16;
        constexpr int kCmdLoiterTime = 19;
        constexpr int kCmdRtl        = 20;
        constexpr int kCmdLand       = 21;
        constexpr int kCmdTakeoff    = 22;
        constexpr int kFrameMission  = 2;  // MAV_FRAME_MISSION: no position
        constexpr int kFrameRelAlt   = 6;  // MAV_FRAME_GLOBAL_RELATIVE_ALT_INT

        const ImVec4 kGreen  = ImVec4(0.16f, 0.80f, 0.40f, 1.0f);
        const ImVec4 kYellow = ImVec4(0.95f, 0.75f, 0.20f, 1.0f);
        const ImVec4 kRed    = ImVec4(0.95f, 0.30f, 0.30f, 1.0f);

        using Item = backend::MissionItem;

        std::unique_ptr<MapWidget> g_map;
        bool g_had_vehicle = false;

        // The plan being edited. Items keep the vehicle's seq while unmodified.
        std::vector<Item> g_plan;
        bool g_dirty = false;            // differs from the vehicle's mission
        bool g_vehicle_changed = false;  // vehicle mission changed while we had unsaved edits
        bool g_force_sync = false;       // take the next vehicle mission even if dirty (after Upload/Download)
        int  g_synced_version = -1;
        int  g_selected = -1;            // index into g_plan
        int  g_plan_rev = 0;             // bumped on every edit; the map is rebuilt when it changes
        int  g_map_rev = -1;
        std::vector<int> g_map_to_plan;  // map waypoint index -> g_plan index
        bool g_edit_mode = true;
        unsigned g_next_uid = 2;         // item ids for the node editor; 1 is the Home/Start node
        bool g_graph_fit_pending = true;  // zoom the node editor to its content on the next frame

        // Export
        std::string g_export_status;
        bool g_export_ok = true;
        std::string g_export_path;

        // ---- Geometry ----

        double DistanceM(double lat1, double lon1, double lat2, double lon2)
        {
            const double p1 = lat1 * kPi / 180.0, p2 = lat2 * kPi / 180.0;
            const double dp = p2 - p1, dl = (lon2 - lon1) * kPi / 180.0;
            const double a = std::sin(dp / 2) * std::sin(dp / 2)
                           + std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
            return 2.0 * kEarthRadiusM * std::asin(std::min(1.0, std::sqrt(a)));
        }

        double BearingDeg(double lat1, double lon1, double lat2, double lon2)
        {
            const double p1 = lat1 * kPi / 180.0, p2 = lat2 * kPi / 180.0;
            const double dl = (lon2 - lon1) * kPi / 180.0;
            const double y = std::sin(dl) * std::cos(p2);
            const double x = std::cos(p1) * std::sin(p2) - std::sin(p1) * std::cos(p2) * std::cos(dl);
            return std::fmod(std::atan2(y, x) * 180.0 / kPi + 360.0, 360.0);
        }

        std::string FormatDistance(double m)
        {
            char buf[32];
            if (m >= 1000.0) snprintf(buf, sizeof(buf), "%.2f km", m / 1000.0);
            else             snprintf(buf, sizeof(buf), "%.0f m", m);
            return buf;
        }

        bool HasHome()
        {
            return std::isfinite(backend::Home_latitude.load()) && std::isfinite(backend::Home_longitude.load());
        }

        // ---- Plan helpers ----

        const char* CommandName(int command)
        {
            switch (command) {
                case 16:   return "Waypoint";
                case 17:   return "Loiter";
                case 18:   return "Loiter (turns)";
                case 19:   return "Loiter (time)";
                case 20:   return "Return to launch";
                case 21:   return "Land";
                case 22:   return "Takeoff";
                case 31:   return "Loiter to altitude";
                case 82:   return "Spline waypoint";
                case 84:   return "VTOL takeoff";
                case 85:   return "VTOL land";
                case 93:   return "Delay";
                case 112:  return "Wait";
                case 177:  return "Jump";
                case 178:  return "Change speed";
                case 183:  return "Set servo";
                case 201:  return "Region of interest";
                case 206:  return "Camera trigger";
                case 2000: return "Start capture";
                case 2001: return "Stop capture";
            }
            return nullptr;
        }

        std::string ItemName(const Item& it)
        {
            if (const char* n = CommandName(it.command)) return n;
            return "Command " + std::to_string(it.command);
        }

        void MarkEdited()
        {
            g_dirty = true;
            ++g_plan_rev;
        }

        // A sensible position for an item that needs one: the previous positioned item,
        // otherwise home, otherwise the map center.
        void DefaultPosition(int index, double& lat, double& lon)
        {
            for (int i = std::min(index, int(g_plan.size())) - 1; i >= 0; --i)
                if (g_plan[i].positional) { lat = g_plan[i].latitude_deg; lon = g_plan[i].longitude_deg; return; }
            if (HasHome()) { lat = backend::Home_latitude; lon = backend::Home_longitude; return; }
            lat = g_map->centerLat();
            lon = g_map->centerLon();
        }

        void SetCommand(Item& it, int index, int command)
        {
            const bool was_positional = it.positional;
            it.command = command;
            it.positional = backend::is_positional_command(command);
            if (it.positional) {
                it.frame = kFrameRelAlt;
                if (!was_positional) {
                    DefaultPosition(index, it.latitude_deg, it.longitude_deg);
                    it.altitude_m = command == kCmdLand ? 0.0f : Settings::Get().default_altitude_m;
                }
            }
            else {
                it.frame = kFrameMission;
            }
            if (command == kCmdLand)
                it.altitude_m = 0.0f;
            it.param1 = it.param2 = it.param3 = it.param4 = 0.0f;
        }

        void InsertItem(Item it)
        {
            const int at = (g_selected >= 0 && g_selected < int(g_plan.size())) ? g_selected + 1 : int(g_plan.size());
            it.seq = -1;
            it.uid = g_next_uid++;
            g_plan.insert(g_plan.begin() + at, it);
            g_selected = at;
            MarkEdited();
        }

        void AddWaypointAt(double lat, double lon)
        {
            bool any_positional = false;
            for (const Item& it : g_plan) any_positional |= it.positional;

            Item it;
            it.command = (!any_positional && Settings::Get().first_item_takeoff) ? kCmdTakeoff : kCmdWaypoint;
            it.frame = kFrameRelAlt;
            it.positional = true;
            it.latitude_deg = lat;
            it.longitude_deg = lon;
            it.altitude_m = Settings::Get().default_altitude_m;
            InsertItem(it);
        }

        void DeleteItem(int index)
        {
            if (index < 0 || index >= int(g_plan.size()))
                return;
            g_plan.erase(g_plan.begin() + index);
            if (g_selected >= int(g_plan.size())) g_selected = int(g_plan.size()) - 1;
            MarkEdited();
        }

        void MoveItem(int index, int delta)
        {
            const int to = index + delta;
            if (index < 0 || to < 0 || index >= int(g_plan.size()) || to >= int(g_plan.size()))
                return;
            std::swap(g_plan[index], g_plan[to]);
            g_selected = to;
            MarkEdited();
        }

        // Total path length: home -> items -> (home again for return-to-launch).
        double PlanDistanceM()
        {
            double total = 0.0;
            bool have_prev = HasHome();
            double plat = backend::Home_latitude, plon = backend::Home_longitude;
            for (const Item& it : g_plan) {
                if (it.positional) {
                    if (have_prev) total += DistanceM(plat, plon, it.latitude_deg, it.longitude_deg);
                    plat = it.latitude_deg; plon = it.longitude_deg; have_prev = true;
                }
                else if (it.command == kCmdRtl && have_prev && HasHome()) {
                    total += DistanceM(plat, plon, backend::Home_latitude, backend::Home_longitude);
                    plat = backend::Home_latitude; plon = backend::Home_longitude;
                }
            }
            return total;
        }

        // Map index of the waypoint the vehicle is flying to; -1 unknown, size() if finished.
        int NextMapIndex()
        {
            if (g_dirty || g_map_to_plan.empty())
                return -1;  // the plan on screen isn't what the vehicle flies
            const int current = backend::Mission_current;
            if (current < 0)
                return -1;
            for (size_t m = 0; m < g_map_to_plan.size(); ++m)
                if (g_plan[g_map_to_plan[m]].seq >= current)
                    return int(m);
            return int(g_map_to_plan.size());
        }

        int PlanToMap(int plan_index)
        {
            for (size_t m = 0; m < g_map_to_plan.size(); ++m)
                if (g_map_to_plan[m] == plan_index) return int(m);
            return -1;
        }

        void RebuildMapMission()
        {
            std::vector<MapWaypoint> wps;
            g_map_to_plan.clear();
            for (size_t i = 0; i < g_plan.size(); ++i) {
                const Item& it = g_plan[i];
                if (!it.positional) continue;
                MapWaypoint w;
                w.lat = it.latitude_deg;
                w.lon = it.longitude_deg;
                w.alt = it.altitude_m;
                w.seq = it.seq;
                w.label = int(i) + 1;  // same numbers as the item list
                wps.push_back(w);
                g_map_to_plan.push_back(int(i));
            }
            g_map->setMission(std::move(wps));
            g_map_rev = g_plan_rev;
        }

        void TakeVehicleMission()
        {
            g_plan = backend::mission_items();
            for (Item& it : g_plan)
                it.uid = g_next_uid++;
            g_dirty = false;
            g_vehicle_changed = false;
            g_force_sync = false;
            g_selected = -1;
            ++g_plan_rev;
            g_graph_fit_pending = true;
            bool any_positional = false;
            for (const Item& it : g_plan) any_positional |= it.positional;
            if (any_positional) {
                g_map->setFollowVehicle(false);
                g_map->fitAll();
            }
        }

        // ---- Map area ----

        void MapToolbar()
        {
            if (g_edit_mode) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
            if (ImGui::Button(g_edit_mode ? "Editing: ON" : "Editing: OFF"))
                g_edit_mode = !g_edit_mode;
            if (g_edit_mode) ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("ON: click the map to add a waypoint, drag waypoints to move them.\n"
                                  "OFF: clicks drop a pin to read coordinates.");

            ImGui::SameLine();
            bool follow = g_map->followVehicle();
            ImGui::BeginDisabled(!g_map->hasVehicle());
            if (ImGui::Checkbox("Follow", &follow))
                g_map->setFollowVehicle(follow);
            ImGui::EndDisabled();

            ImGui::SameLine();
            if (ImGui::Button("Fit all"))
                g_map->fitAll();
            ImGui::SameLine();
            if (ImGui::Button("Clear trail"))
                g_map->clearTrail();
            if (g_map->hasPickedPoint()) {
                ImGui::SameLine();
                if (ImGui::Button("Clear pin"))
                    g_map->clearPickedPoint();
            }

            // Tile source, right-aligned on the row if it fits
            int& source = Settings::Get().tile_source;
            const float combo_w = std::min(260.0f * ImGui::GetIO().FontGlobalScale,
                                           ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x);
            ImGui::SameLine();
            if (ImGui::GetContentRegionAvail().x < combo_w)
                ImGui::NewLine();
            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - combo_w);
            ImGui::SetNextItemWidth(combo_w);
            if (ImGui::BeginCombo("##tiles", MapWidget::sourceAt(source).name)) {
                for (int i = 0; i < MapWidget::sourceCount(); ++i)
                    if (ImGui::Selectable(MapWidget::sourceAt(i).name, i == source))
                        source = i;
                ImGui::EndCombo();
            }
        }

        void MapStatusLine()
        {
            const double lat = backend::Position_latitude;
            const double lon = backend::Position_longitude;
            const bool has_pos = std::isfinite(lat) && std::isfinite(lon);

            if (has_pos) ImGui::Text("Vehicle  %.6f, %.6f", lat, lon);
            else         ImGui::TextDisabled(backend::mavsdk_connected ? "Vehicle  waiting for position..."
                                                                       : "No vehicle connected");

            if (g_edit_mode) {
                ImGui::SameLine(0.0f, 24.0f);
                ImGui::TextDisabled("Click the map to add a waypoint");
            }

            const int next = NextMapIndex();
            const int n = int(g_map_to_plan.size());
            if (next < 0 || n == 0)
                return;
            ImGui::SameLine(0.0f, 24.0f);
            if (next >= n) {
                ImGui::TextColored(kGreen, "Mission complete");
                return;
            }
            const Item& w = g_plan[g_map_to_plan[next]];
            if (has_pos)
                ImGui::TextColored(kGreen, "Next: item %d   %s   bearing %03.0f\xC2\xB0   alt %.0f m",
                                   g_map_to_plan[next] + 1,
                                   FormatDistance(DistanceM(lat, lon, w.latitude_deg, w.longitude_deg)).c_str(),
                                   BearingDeg(lat, lon, w.latitude_deg, w.longitude_deg), w.altitude_m);
            else
                ImGui::TextColored(kGreen, "Next: item %d", g_map_to_plan[next] + 1);
        }

        void HandleMapEvents()
        {
            const MapEvents& ev = g_map->events();
            if (ev.clickedWaypoint >= 0 && ev.clickedWaypoint < int(g_map_to_plan.size()))
                g_selected = g_map_to_plan[ev.clickedWaypoint];

            if (ev.draggedWaypoint >= 0 && ev.draggedWaypoint < int(g_map_to_plan.size())) {
                Item& it = g_plan[g_map_to_plan[ev.draggedWaypoint]];
                it.latitude_deg = ev.dragLat;
                it.longitude_deg = ev.dragLon;
                g_selected = g_map_to_plan[ev.draggedWaypoint];
                g_dirty = true;
                if (ev.dragEnded)
                    ++g_plan_rev;  // the widget already moved the marker while dragging
            }

            if (ev.clickedEmpty && g_map->editable())
                AddWaypointAt(ev.clickLat, ev.clickLon);
        }

        // ---- Node editor ----
        //
        // Every mission item is a node; links show the flight order, starting at the Home/Start node.
        // Dragging a new link from A's "Out" pin to B's "In" pin moves B right after A.

        constexpr unsigned kStartUid = 1;

        ed::EditorContext* g_graph = nullptr;
        std::unordered_set<unsigned> g_placed;              // nodes that already have a position
        std::unordered_map<unsigned, ImVec2> g_place_at;    // requested position for new nodes
        unsigned g_graph_selected_uid = 0;                  // selection last synced with the graph
        double g_last_flow = 0.0;

        ed::NodeId GraphNode(unsigned uid) { return ed::NodeId(uintptr_t(uid) * 8 + 1); }
        ed::PinId  InPin(unsigned uid)     { return ed::PinId(uintptr_t(uid) * 8 + 2); }
        ed::PinId  OutPin(unsigned uid)    { return ed::PinId(uintptr_t(uid) * 8 + 3); }
        ed::LinkId LinkInto(unsigned uid)  { return ed::LinkId(uintptr_t(uid) * 8 + 4); }
        unsigned   UidOf(uintptr_t id)     { return unsigned(id / 8); }
        int        KindOf(uintptr_t id)    { return int(id % 8); }  // 1 node, 2 in-pin, 3 out-pin, 4 link

        int IndexOfUid(unsigned uid)
        {
            for (size_t i = 0; i < g_plan.size(); ++i)
                if (g_plan[i].uid == uid) return int(i);
            return -1;
        }

        // Snake grid: left to right, then right to left on the next row, so links stay short.
        ImVec2 AutoPosition(int slot)
        {
            const int cols = 4;
            const int row = slot / cols;
            int col = slot % cols;
            if (row % 2) col = cols - 1 - col;
            return ImVec2(col * 260.0f, row * 200.0f);
        }

        ImVec4 HeaderColor(int command)
        {
            switch (command) {
                case kCmdTakeoff: case 84:          return ImVec4(0.20f, 0.55f, 0.95f, 1.0f);
                case kCmdLand: case 85:             return ImVec4(0.90f, 0.35f, 0.25f, 1.0f);
                case kCmdRtl:                       return ImVec4(0.60f, 0.30f, 0.85f, 1.0f);
                case 17: case 18: case kCmdLoiterTime: case 31: return ImVec4(0.15f, 0.65f, 0.60f, 1.0f);
                case kCmdWaypoint: case 82:         return ImVec4(0.85f, 0.65f, 0.15f, 1.0f);
            }
            return ImVec4(0.45f, 0.45f, 0.50f, 1.0f);
        }

        // Moves item `uid` to right after `after_uid` (kStartUid = to the front).
        void MoveAfter(unsigned after_uid, unsigned uid)
        {
            const int from = IndexOfUid(uid);
            if (from < 0) return;
            const int after = after_uid == kStartUid ? -1 : IndexOfUid(after_uid);
            if (after_uid != kStartUid && after < 0) return;
            if (after + 1 == from) return;  // already there
            Item it = g_plan[from];
            g_plan.erase(g_plan.begin() + from);
            const int to = after_uid == kStartUid ? 0 : IndexOfUid(after_uid) + 1;
            g_plan.insert(g_plan.begin() + to, it);
            g_selected = to;
            MarkEdited();
        }

        void ArrangeGraph()
        {
            g_placed.clear();
            g_place_at.clear();
            g_graph_fit_pending = true;
        }

        void NodeHeader(unsigned uid, const ImVec4& color, ImVec2 header_max)
        {
            ImDrawList* dl = ed::GetNodeBackgroundDrawList(GraphNode(uid));
            if (!dl) return;
            const ImVec2 pos = ed::GetNodePosition(GraphNode(uid));
            const ImVec2 size = ed::GetNodeSize(GraphNode(uid));
            const float rounding = ed::GetStyle().NodeRounding;
            dl->AddRectFilled(ImVec2(pos.x + 1, pos.y + 1), ImVec2(pos.x + size.x - 1, header_max.y + 4),
                              ImGui::GetColorU32(ImVec4(color.x, color.y, color.z, 0.75f)), rounding,
                              ImDrawFlags_RoundCornersTop);
        }

        void PlaceNode(unsigned uid, int slot)
        {
            if (g_placed.count(uid)) return;
            auto it = g_place_at.find(uid);
            ed::SetNodePosition(GraphNode(uid), it != g_place_at.end() ? it->second : AutoPosition(slot));
            if (it != g_place_at.end()) g_place_at.erase(it);
            g_placed.insert(uid);
        }

        void StartNode(float width)
        {
            PlaceNode(kStartUid, 0);
            ed::BeginNode(GraphNode(kStartUid));
            ImGui::TextUnformatted("H   Home / Start");
            const ImVec2 header_max = ImGui::GetItemRectMax();
            ImGui::Dummy(ImVec2(width, 4));
            if (HasHome()) ImGui::TextDisabled("%.6f, %.6f", backend::Home_latitude.load(), backend::Home_longitude.load());
            else           ImGui::TextDisabled("Home not known yet");
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + width - ImGui::CalcTextSize("Out >").x);
            ed::BeginPin(OutPin(kStartUid), ed::PinKind::Output);
            ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
            ImGui::TextUnformatted("Out >");
            ed::EndPin();
            ed::EndNode();
            NodeHeader(kStartUid, ImVec4(0.55f, 0.25f, 0.80f, 1.0f), header_max);
        }

        void ItemNode(int index, int next_plan, float width)
        {
            Item& it = g_plan[index];
            const unsigned uid = it.uid;
            PlaceNode(uid, index + 1);

            const bool is_next = index == next_plan;
            if (is_next) ed::PushStyleColor(ed::StyleColor_NodeBorder, kGreen);
            ed::BeginNode(GraphNode(uid));
            ImGui::PushID(int(uid));

            const std::string name = ItemName(it);
            if (is_next) ImGui::Text("%d   %s  (next)", index + 1, name.c_str());
            else         ImGui::Text("%d   %s", index + 1, name.c_str());
            const ImVec2 header_max = ImGui::GetItemRectMax();
            ImGui::Dummy(ImVec2(width, 4));

            // Pins
            ed::BeginPin(InPin(uid), ed::PinKind::Input);
            ed::PinPivotAlignment(ImVec2(0.0f, 0.5f));
            ImGui::TextUnformatted("> In");
            ed::EndPin();
            ImGui::SameLine(width - ImGui::CalcTextSize("Out >").x + ImGui::GetStyle().ItemSpacing.x);
            ed::BeginPin(OutPin(uid), ed::PinKind::Output);
            ed::PinPivotAlignment(ImVec2(1.0f, 0.5f));
            ImGui::TextUnformatted("Out >");
            ed::EndPin();

            // Body
            if (it.positional) {
                ImGui::TextDisabled("%.6f, %.6f", it.latitude_deg, it.longitude_deg);
                ImGui::SetNextItemWidth(width);
                if (ImGui::DragFloat("##alt", &it.altitude_m, 0.5f, 0.0f, 5000.0f, "Alt %.1f m"))
                    MarkEdited();
            }
            if (it.command == kCmdWaypoint || it.command == kCmdLoiterTime) {
                ImGui::SetNextItemWidth(width);
                if (ImGui::DragFloat("##hold", &it.param1, 0.5f, 0.0f, 3600.0f,
                                     it.command == kCmdWaypoint ? "Hold %.0f s" : "Loiter %.0f s"))
                    MarkEdited();
            }
            else if (it.command == kCmdRtl) {
                ImGui::TextDisabled("Flies back to home");
            }
            else if (!it.positional) {
                ImGui::TextDisabled("Params %.1f %.1f %.1f %.1f", it.param1, it.param2, it.param3, it.param4);
            }

            ImGui::PopID();
            ed::EndNode();
            if (is_next) ed::PopStyleColor();
            NodeHeader(uid, HeaderColor(it.command), header_max);
        }

        void GraphLinks(int next_plan)
        {
            unsigned prev = kStartUid;
            for (int i = 0; i < int(g_plan.size()); ++i) {
                const unsigned uid = g_plan[i].uid;
                ImVec4 color = ImVec4(1.0f, 0.78f, 0.16f, 1.0f);              // still to fly
                if (next_plan >= 0 && i < next_plan) color = ImVec4(0.6f, 0.6f, 0.6f, 0.7f);  // flown
                if (i == next_plan) color = kGreen;
                ed::Link(LinkInto(uid), OutPin(prev), InPin(uid), color, i == next_plan ? 3.5f : 2.5f);
                prev = uid;
            }
            // Animate the leg being flown
            if (next_plan >= 0 && next_plan < int(g_plan.size()) && ImGui::GetTime() - g_last_flow > 1.2) {
                ed::Flow(LinkInto(g_plan[next_plan].uid));
                g_last_flow = ImGui::GetTime();
            }
        }

        void GraphCreateAndDelete()
        {
            if (ed::BeginCreate(ImVec4(1, 1, 1, 1), 2.0f)) {
                ed::PinId a, b;
                if (ed::QueryNewLink(&a, &b)) {
                    uintptr_t from = a.Get(), to = b.Get();
                    if (KindOf(from) == 2 && KindOf(to) == 3) std::swap(from, to);  // dragged from an In pin
                    const bool valid = KindOf(from) == 3 && KindOf(to) == 2
                                    && UidOf(from) != UidOf(to) && UidOf(to) != kStartUid;
                    if (!valid)
                        ed::RejectNewItem(ImVec4(0.95f, 0.3f, 0.3f, 1.0f), 2.0f);
                    else if (ed::AcceptNewItem(kGreen, 3.0f))
                        MoveAfter(UidOf(from), UidOf(to));
                }
            }
            ed::EndCreate();

            std::vector<unsigned> doomed;
            if (ed::BeginDelete()) {
                ed::LinkId link;
                while (ed::QueryDeletedLink(&link))
                    ed::RejectDeletedItem();  // the order is changed by drawing new links instead
                ed::NodeId node;
                while (ed::QueryDeletedNode(&node)) {
                    const unsigned uid = UidOf(node.Get());
                    if (uid == kStartUid) ed::RejectDeletedItem();
                    else if (ed::AcceptDeletedItem()) doomed.push_back(uid);
                }
            }
            ed::EndDelete();
            for (unsigned uid : doomed) {
                DeleteItem(IndexOfUid(uid));
                g_placed.erase(uid);
            }
        }

        // New item for the graph menus: just east of the item it follows.
        void AddFromGraph(int command, ImVec2 canvas_pos)
        {
            Item it;
            SetCommand(it, g_selected + 1, command);
            if (it.positional) {
                double lat, lon;
                DefaultPosition(g_selected + 1, lat, lon);
                it.latitude_deg = lat;
                it.longitude_deg = lon + (g_plan.empty() ? 0.0 : 0.0005);  // ~35 m east
            }
            InsertItem(it);
            g_place_at[g_plan[g_selected].uid] = canvas_pos;
        }

        void GraphMenus()
        {
            ed::Suspend();
            static unsigned menu_uid = 0;
            static ImVec2 menu_canvas_pos;
            ed::NodeId node;
            if (ed::ShowNodeContextMenu(&node)) {
                menu_uid = UidOf(node.Get());
                ImGui::OpenPopup("##graph_node_menu");
            }
            else if (ed::ShowBackgroundContextMenu()) {
                menu_canvas_pos = ed::ScreenToCanvas(ImGui::GetMousePos());
                ImGui::OpenPopup("##graph_menu");
            }

            if (ImGui::BeginPopup("##graph_node_menu")) {
                const int index = IndexOfUid(menu_uid);
                if (menu_uid == kStartUid) {
                    ImGui::TextDisabled("Home / Start");
                }
                else if (index >= 0) {
                    ImGui::TextDisabled("Item %d: %s", index + 1, ItemName(g_plan[index]).c_str());
                    ImGui::Separator();
                    if (ImGui::MenuItem("Move earlier", nullptr, false, index > 0)) MoveItem(index, -1);
                    if (ImGui::MenuItem("Move later", nullptr, false, index + 1 < int(g_plan.size()))) MoveItem(index, +1);
                    if (ImGui::MenuItem("Delete")) { DeleteItem(index); g_placed.erase(menu_uid); }
                }
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup("##graph_menu")) {
                ImGui::TextDisabled("Insert after the selected item");
                ImGui::Separator();
                if (ImGui::MenuItem("Add waypoint"))         AddFromGraph(kCmdWaypoint, menu_canvas_pos);
                if (ImGui::MenuItem("Add takeoff"))          AddFromGraph(kCmdTakeoff, menu_canvas_pos);
                if (ImGui::MenuItem("Add loiter (time)"))    AddFromGraph(kCmdLoiterTime, menu_canvas_pos);
                if (ImGui::MenuItem("Add land"))             AddFromGraph(kCmdLand, menu_canvas_pos);
                if (ImGui::MenuItem("Add return to launch")) AddFromGraph(kCmdRtl, menu_canvas_pos);
                ImGui::Separator();
                if (ImGui::MenuItem("Arrange nodes"))        ArrangeGraph();
                if (ImGui::MenuItem("Zoom to fit"))          g_graph_fit_pending = true;
                ImGui::EndPopup();
            }
            ed::Resume();
        }

        void GraphSelectionSync()
        {
            // Graph -> plan
            if (ed::HasSelectionChanged()) {
                ed::NodeId selected[2];
                const int n = ed::GetSelectedNodes(selected, 2);
                const int index = n == 1 ? IndexOfUid(UidOf(selected[0].Get())) : -1;
                if (n == 1 && index >= 0) g_selected = index;
                else if (n == 0)          g_selected = -1;
                g_graph_selected_uid = (g_selected >= 0) ? g_plan[g_selected].uid : 0;
            }
            // Plan (list or map) -> graph
            const unsigned want = (g_selected >= 0 && g_selected < int(g_plan.size())) ? g_plan[g_selected].uid : 0;
            if (want != g_graph_selected_uid) {
                if (want) ed::SelectNode(GraphNode(want));
                else      ed::ClearSelection();
                g_graph_selected_uid = want;
            }
        }

        void NodeEditorView()
        {
            if (!g_graph) {
                ed::Config config;
                config.SettingsFile = nullptr;  // layout is derived from the mission, nothing to save
                g_graph = ed::CreateEditor(&config);
            }

            if (ImGui::Button("Arrange"))
                ArrangeGraph();
            ImGui::SameLine();
            if (ImGui::Button("Zoom to fit"))
                g_graph_fit_pending = true;
            ImGui::SameLine(0.0f, 16.0f);
            ImGui::TextDisabled("Drag Out -> In to reorder  \xC2\xB7  Del deletes  \xC2\xB7  right-click to add  \xC2\xB7  right-drag pans");

            const int next_map = NextMapIndex();
            int next_plan = -1;
            if (next_map >= 0 && next_map < int(g_map_to_plan.size())) next_plan = g_map_to_plan[next_map];
            else if (next_map >= int(g_map_to_plan.size()) && !g_map_to_plan.empty()) next_plan = int(g_plan.size());

            ed::SetCurrentEditor(g_graph);
            ed::Begin("##mission_graph");
            const float width = 190.0f * ImGui::GetIO().FontGlobalScale;
            StartNode(width);
            for (int i = 0; i < int(g_plan.size()); ++i)
                ItemNode(i, next_plan, width);
            GraphLinks(next_plan);
            GraphCreateAndDelete();
            GraphMenus();
            GraphSelectionSync();
            if (g_graph_fit_pending && ImGui::GetFrameCount() > 2) {
                ed::NavigateToContent(0.0f);
                g_graph_fit_pending = false;
            }
            ed::End();
            ed::SetCurrentEditor(nullptr);
        }

        // ---- Export ----

        void ExportJson()
        {
            char name[64];
            const std::time_t now = std::time(nullptr);
            std::strftime(name, sizeof(name), "mission_%Y%m%d_%H%M.json", std::localtime(&now));

            std::string path;
            switch (FileDialog::Save("Export mission as JSON", name, "json", path)) {
                case FileDialog::Result::Cancelled:
                    return;
                case FileDialog::Result::Unavailable:
                    g_export_ok = false;
                    g_export_status = path;
                    return;
                case FileDialog::Result::Ok:
                    break;
            }

            MissionExport::PlanInfo info;
            info.cruise_speed_m_s = Settings::Get().plan_speed_m_s;
            if (HasHome()) {
                info.home_lat = backend::Home_latitude;
                info.home_lon = backend::Home_longitude;
                const float amsl = backend::Home_altitude_amsl;
                info.home_alt_amsl = std::isfinite(amsl) ? amsl : 0.0;
            }
            else {
                for (const Item& it : g_plan)
                    if (it.positional) { info.home_lat = it.latitude_deg; info.home_lon = it.longitude_deg; break; }
            }

            std::string error;
            if (MissionExport::WriteFile(path, MissionExport::ToPlanJson(g_plan, info), error)) {
                g_export_ok = true;
                g_export_path = path;
                g_export_status = "Saved " + std::filesystem::path(path).filename().string();
            }
            else {
                g_export_ok = false;
                g_export_status = error;
            }
        }

        // Bottom-right corner of the Mission Plan tab
        void ExportFooter()
        {
            const ImGuiStyle& style = ImGui::GetStyle();
            const char* label = "Export JSON...";
            const float button_w = ImGui::CalcTextSize(label).x + style.FramePadding.x * 4.0f;
            const float y = ImGui::GetWindowHeight() - ImGui::GetFrameHeight() - style.WindowPadding.y;
            ImGui::SetCursorPosY(y);
            ImGui::Separator();
            ImGui::SetCursorPosY(y);

            if (!g_export_status.empty()) {
                ImGui::AlignTextToFramePadding();
                ImGui::PushClipRect(ImGui::GetCursorScreenPos(),
                                    ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x - button_w - 8.0f,
                                           ImGui::GetCursorScreenPos().y + ImGui::GetFrameHeight()), true);
                ImGui::TextColored(g_export_ok ? kGreen : kRed, "%s", g_export_status.c_str());
                ImGui::PopClipRect();
                if (ImGui::IsItemHovered() && !g_export_path.empty() && g_export_ok)
                    ImGui::SetTooltip("%s", g_export_path.c_str());
                ImGui::SameLine();
            }

            ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMax().x - button_w);
            ImGui::BeginDisabled(g_plan.empty());
            if (ImGui::Button(label, ImVec2(button_w, 0)))
                ExportJson();
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip(g_plan.empty() ? "The plan is empty"
                                                 : "Save the plan as a JSON file (QGroundControl .plan format)");
        }

        // ---- Mission panel ----

        void PlanHeader()
        {
            ImGui::Text("Mission");
            ImGui::SameLine();
            if (g_plan.empty() && !g_dirty)  ImGui::TextDisabled("(empty)");
            else if (g_dirty)                ImGui::TextColored(kYellow, "Not uploaded");
            else                             ImGui::TextColored(kGreen, "Same as vehicle");

            ImGui::PushTextWrapPos(0.0f);
            ImGui::TextDisabled("%s", backend::mission_status().c_str());
            ImGui::PopTextWrapPos();

            if (g_vehicle_changed) {
                ImGui::TextColored(kYellow, "The vehicle's mission changed.");
                ImGui::SameLine();
                if (ImGui::SmallButton("Load it"))
                    TakeVehicleMission();
            }
        }

        void PlanActions()
        {
            const bool connected = backend::mavsdk_connected;
            const bool busy = backend::mission_busy();
            const float w = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

            ImGui::BeginDisabled(!connected || busy || g_plan.empty());
            if (ImGui::Button("Upload to vehicle", ImVec2(w, 0))) {
                g_force_sync = true;
                if (!backend::upload_mission(g_plan))
                    g_force_sync = false;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(!connected || busy);
            if (ImGui::Button("Download", ImVec2(w, 0))) {
                if (g_dirty)
                    ImGui::OpenPopup("Discard plan?");
                else {
                    g_force_sync = true;
                    backend::reload_mission();
                }
            }
            ImGui::EndDisabled();
            if (!connected && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("Connect to a vehicle first");

            if (ImGui::BeginPopupModal("Discard plan?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("Your plan has changes that are not uploaded.\nReplace it with the vehicle's mission?");
                if (ImGui::Button("Replace", ImVec2(120, 0))) {
                    g_force_sync = true;
                    backend::reload_mission();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }

            ImGui::BeginDisabled(g_plan.empty());
            if (ImGui::Button("+ Return to launch", ImVec2(w, 0))) {
                Item it;
                it.command = kCmdRtl;
                it.frame = kFrameMission;
                it.positional = false;
                g_selected = int(g_plan.size()) - 1;  // always at the end
                InsertItem(it);
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(g_plan.empty());
            if (ImGui::Button("Clear plan", ImVec2(w, 0))) {
                g_plan.clear();
                g_selected = -1;
                MarkEdited();
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(!connected || busy || backend::Mission_total == 0);
            if (ImGui::Button("Delete mission on vehicle...", ImVec2(-FLT_MIN, 0)))
                ImGui::OpenPopup("Delete vehicle mission?");
            ImGui::EndDisabled();
            if (ImGui::BeginPopupModal("Delete vehicle mission?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("Remove the mission stored on the vehicle?\nYour plan here is kept.");
                if (ImGui::Button("Delete", ImVec2(120, 0))) {
                    backend::clear_vehicle_mission();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel", ImVec2(120, 0)))
                    ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
        }

        void PlanSummary()
        {
            int waypoints = 0;
            for (const Item& it : g_plan) waypoints += it.positional;
            const double dist = PlanDistanceM();
            const double t = dist / std::max(0.5f, Settings::Get().plan_speed_m_s);
            ImGui::TextDisabled("%d items, %d with position", int(g_plan.size()), waypoints);
            ImGui::TextDisabled("Distance %s   ~%d:%02d min at %.0f m/s", FormatDistance(dist).c_str(),
                                int(t) / 60, int(t) % 60, Settings::Get().plan_speed_m_s);

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.45f);
            ImGui::DragFloat("New waypoint altitude", &Settings::Get().default_altitude_m, 0.5f, 1.0f, 1000.0f, "%.0f m");
        }

        void ItemList(float height)
        {
            const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY
                                        | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_SizingStretchProp;
            if (!ImGui::BeginTable("##items", 4, flags, ImVec2(0, height)))
                return;
            ImGui::TableSetupScrollFreeze(0, 1);
            ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, ImGui::CalcTextSize("000").x);
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 2.0f);
            ImGui::TableSetupColumn("Alt", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Leg", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableHeadersRow();

            const int next_map = NextMapIndex();
            const int next_plan = (next_map >= 0 && next_map < int(g_map_to_plan.size())) ? g_map_to_plan[next_map] : -1;

            bool have_prev = HasHome();
            double plat = backend::Home_latitude, plon = backend::Home_longitude;
            for (int i = 0; i < int(g_plan.size()); ++i) {
                const Item& it = g_plan[i];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                char id[16];
                snprintf(id, sizeof(id), "%d##row", i + 1);
                if (ImGui::Selectable(id, g_selected == i, ImGuiSelectableFlags_SpanAllColumns))
                    g_selected = i;

                ImGui::TableSetColumnIndex(1);
                const std::string name = ItemName(it);
                if (i == next_plan) ImGui::TextColored(kGreen, "%s  (next)", name.c_str());
                else if (it.positional || it.command == kCmdRtl) ImGui::TextUnformatted(name.c_str());
                else ImGui::TextDisabled("%s", name.c_str());

                ImGui::TableSetColumnIndex(2);
                if (it.positional) ImGui::Text("%.0f m", it.altitude_m);
                else               ImGui::TextDisabled("-");

                ImGui::TableSetColumnIndex(3);
                if (it.positional) {
                    if (have_prev) ImGui::TextDisabled("%s", FormatDistance(DistanceM(plat, plon, it.latitude_deg, it.longitude_deg)).c_str());
                    plat = it.latitude_deg; plon = it.longitude_deg; have_prev = true;
                }
            }
            if (g_plan.empty()) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled(g_edit_mode ? "Click the map to add waypoints" : "Turn editing on to add waypoints");
            }
            ImGui::EndTable();
        }

        void ItemEditor()
        {
            if (g_selected < 0 || g_selected >= int(g_plan.size())) {
                ImGui::TextDisabled("Select an item to edit it.");
                return;
            }
            Item& it = g_plan[g_selected];
            ImGui::Text("Item %d", g_selected + 1);
            if (it.seq >= 0 && !g_dirty) {
                ImGui::SameLine();
                ImGui::TextDisabled("(vehicle seq %d)", it.seq);
            }

            const float label_x = ImGui::GetContentRegionAvail().x * 0.32f;
            auto label = [&](const char* text) {
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(text);
                ImGui::SameLine(label_x);
                ImGui::SetNextItemWidth(-FLT_MIN);
            };

            static const int kEditable[] = {kCmdWaypoint, kCmdTakeoff, kCmdLoiterTime, kCmdLand, kCmdRtl};
            label("Type");
            if (ImGui::BeginCombo("##type", ItemName(it).c_str())) {
                for (int cmd : kEditable)
                    if (ImGui::Selectable(CommandName(cmd), it.command == cmd) && it.command != cmd) {
                        SetCommand(it, g_selected, cmd);
                        MarkEdited();
                    }
                ImGui::EndCombo();
            }

            if (it.positional) {
                label("Latitude");
                if (ImGui::InputDouble("##lat", &it.latitude_deg, 0.0, 0.0, "%.7f")) {
                    it.latitude_deg = std::clamp(it.latitude_deg, -85.0, 85.0);
                    MarkEdited();
                }
                label("Longitude");
                if (ImGui::InputDouble("##lon", &it.longitude_deg, 0.0, 0.0, "%.7f")) {
                    it.longitude_deg = std::clamp(it.longitude_deg, -180.0, 180.0);
                    MarkEdited();
                }
                label("Altitude");
                if (ImGui::DragFloat("##alt", &it.altitude_m, 0.5f, 0.0f, 5000.0f, "%.1f m (rel. home)"))
                    MarkEdited();
            }
            if (it.command == kCmdWaypoint) {
                label("Hold time");
                if (ImGui::DragFloat("##hold", &it.param1, 0.5f, 0.0f, 3600.0f, "%.0f s"))
                    MarkEdited();
            }
            else if (it.command == kCmdLoiterTime) {
                label("Loiter time");
                if (ImGui::DragFloat("##loiter", &it.param1, 0.5f, 0.0f, 3600.0f, "%.0f s"))
                    MarkEdited();
            }
            else if (!it.positional && it.command != kCmdRtl) {
                ImGui::TextDisabled("Parameters: %.1f, %.1f, %.1f, %.1f", it.param1, it.param2, it.param3, it.param4);
            }

            const float w = (ImGui::GetContentRegionAvail().x - 2 * ImGui::GetStyle().ItemSpacing.x) / 3.0f;
            ImGui::BeginDisabled(g_selected == 0);
            if (ImGui::Button("Move up", ImVec2(w, 0))) MoveItem(g_selected, -1);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(g_selected + 1 >= int(g_plan.size()));
            if (ImGui::Button("Move down", ImVec2(w, 0))) MoveItem(g_selected, +1);
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, kRed);
            if (ImGui::Button("Delete", ImVec2(w, 0))) DeleteItem(g_selected);
            ImGui::PopStyleColor();
        }

        // Lays itself out in one column when the window is tall, or three columns
        // (plan | items | selected item) when it is wide, e.g. docked along the bottom.
        void MissionPanel()
        {
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            const float footer_h = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y * 2.0f;
            const bool wide = avail.x > avail.y * 1.8f && avail.x > 700.0f * ImGui::GetIO().FontGlobalScale;

            if (!wide) {
                ImGui::BeginChild("##panel_body", ImVec2(0, avail.y - footer_h));
                PlanHeader();
                ImGui::Spacing();
                PlanActions();
                ImGui::Spacing();
                PlanSummary();
                ImGui::SeparatorText("Items");
                const float editor_h = ImGui::GetFrameHeightWithSpacing() * 7.5f;
                ItemList(std::max(ImGui::GetFrameHeight() * 4, ImGui::GetContentRegionAvail().y - editor_h));
                ImGui::SeparatorText("Selected item");
                ItemEditor();
                ImGui::EndChild();
            }
            else {
                const float h = avail.y - footer_h;
                const float spacing = ImGui::GetStyle().ItemSpacing.x * 2.0f;
                const float col = (avail.x - 2.0f * spacing) / 3.0f;

                ImGui::BeginChild("##panel_plan", ImVec2(col, h));
                PlanHeader();
                ImGui::Spacing();
                PlanActions();
                ImGui::Spacing();
                PlanSummary();
                ImGui::EndChild();

                ImGui::SameLine(0.0f, spacing);
                ImGui::BeginChild("##panel_items", ImVec2(col, h));
                ImGui::TextUnformatted("Items");
                ItemList(ImGui::GetContentRegionAvail().y);
                ImGui::EndChild();

                ImGui::SameLine(0.0f, spacing);
                ImGui::BeginChild("##panel_item", ImVec2(0, h));
                ImGui::TextUnformatted("Selected item");
                ItemEditor();
                ImGui::EndChild();
            }
            ExportFooter();
        }


        // Keyboard: Delete removes the selected item, Escape deselects.
        void Shortcuts()
        {
            if (ImGui::GetIO().WantTextInput)
                return;
            if (ImGui::IsKeyPressed(ImGuiKey_Delete) || ImGui::IsKeyPressed(ImGuiKey_Backspace))
                DeleteItem(g_selected);
            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                g_selected = -1;
        }
    }

    std::string TileCacheDir()
    {
#if defined(_WIN32)
        const char* base = std::getenv("LOCALAPPDATA");
        return base ? std::string(base) + "/KAYRA/tiles" : "tile_cache";
#elif defined(__APPLE__)
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/Library/Caches/KAYRA/tiles" : "tile_cache";
#else
        const char* xdg = std::getenv("XDG_CACHE_HOME");
        if (xdg && *xdg) return std::string(xdg) + "/KAYRA/tiles";
        const char* home = std::getenv("HOME");
        return home ? std::string(home) + "/.cache/KAYRA/tiles" : "tile_cache";
#endif
    }

    void ClearTileCache()
    {
        std::error_code ec;
        std::filesystem::remove_all(TileCacheDir(), ec);
    }

    void Update()
    {
        const Settings::Values& s = Settings::Get();
        if (!g_map) {
            g_map = std::make_unique<MapWidget>(TileCacheDir());
            g_map->setCenter(30.0, 20.0);
            g_map->setZoom(2.5);
        }
        if (g_map->sourceIndex() != s.tile_source)
            g_map->setSource(s.tile_source);
        g_map->setShowTrail(s.show_trail);
        g_map->setEditable(g_edit_mode);

        // Vehicle
        const double lat = backend::Position_latitude;
        const double lon = backend::Position_longitude;
        const bool has_pos = std::isfinite(lat) && std::isfinite(lon);
        if (has_pos) {
            float heading = backend::Heading_deg;
            if (!std::isfinite(heading)) heading = backend::Attitude_yaw_deg;
            if (!std::isfinite(heading)) heading = 0.0f;
            if (!g_had_vehicle) {
                // First fix of this connection: start a new trail
                g_map->clearTrail();
                if (s.follow_on_connect) {
                    g_map->setCenter(lat, lon);
                    g_map->setZoom(17.0);
                    g_map->setFollowVehicle(true);
                }
            }
            g_map->setVehicle(lat, lon, heading);
        }
        else if (g_had_vehicle) {
            g_map->clearVehicle();
        }
        g_had_vehicle = has_pos;

        if (HasHome()) g_map->setHome(backend::Home_latitude, backend::Home_longitude);
        else           g_map->clearHome();

        // Vehicle mission -> plan (unless the user is editing)
        const int version = backend::Mission_version;
        if (version != g_synced_version) {
            g_synced_version = version;
            if (!g_dirty || g_force_sync) TakeVehicleMission();
            else                          g_vehicle_changed = true;
        }

        if (g_map_rev != g_plan_rev)
            RebuildMapMission();
        g_map->setSelectedWaypoint(PlanToMap(g_selected));
        g_map->setNextWaypoint(NextMapIndex());
    }

    namespace {
        bool g_layout_built = false;
    }

    void BuildDefaultLayout(unsigned dockspace_id, float width, float height)
    {
        //   +------------------+----------------+
        //   |       Map        |  Node editor   |
        //   +------------------+----------------+
        //   |              Mission              |
        //   +-----------------------------------+
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, ImVec2(width, height));

        ImGuiID top = 0, bottom = 0, left = 0, right = 0;
        ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Down, 0.38f, &bottom, &top);
        ImGui::DockBuilderSplitNode(top, ImGuiDir_Left, 0.55f, &left, &right);

        ImGui::DockBuilderDockWindow(kMapTitle, left);
        ImGui::DockBuilderDockWindow(kGraphTitle, right);
        ImGui::DockBuilderDockWindow(kPanelTitle, bottom);
        ImGui::DockBuilderFinish(dockspace_id);
        g_layout_built = true;
    }

    bool LayoutBuilt() { return g_layout_built; }
    void ResetLayout() { g_layout_built = false; }

    void WindowsMenu()
    {
        Settings::Values& v = Settings::Get();
        ImGui::SeparatorText("Mission Plan");
        ImGui::MenuItem("Map",         nullptr, &v.plan_map);
        ImGui::MenuItem("Node editor", nullptr, &v.plan_graph);
        ImGui::MenuItem("Mission",     nullptr, &v.plan_panel);
        ImGui::Separator();
        if (ImGui::MenuItem("Reset layout")) {
            v.plan_map = v.plan_graph = v.plan_panel = true;
            ResetLayout();
        }
    }

    void ShowWindows()
    {
        Settings::Values& v = Settings::Get();
        const ImGuiWindowFlags canvas_flags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        bool keyboard_here = false;  // Delete/Escape go to the plan (the node editor handles its own)

        if (v.plan_map) {
            if (ImGui::Begin(kMapTitle, &v.plan_map, canvas_flags)) {
                MapToolbar();
                MapStatusLine();
                g_map->draw("##map");
                HandleMapEvents();  // only right after draw(): events belong to this frame
                keyboard_here |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            }
            ImGui::End();
        }

        if (v.plan_graph) {
            if (ImGui::Begin(kGraphTitle, &v.plan_graph, canvas_flags))
                NodeEditorView();
            ImGui::End();
        }

        if (v.plan_panel) {
            if (ImGui::Begin(kPanelTitle, &v.plan_panel, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
                MissionPanel();
                keyboard_here |= ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
            }
            ImGui::End();
        }

        if (keyboard_here)
            Shortcuts();

        // Apply edits made this frame right away (marker numbers, selection)
        if (g_map_rev != g_plan_rev)
            RebuildMapMission();
        g_map->setSelectedWaypoint(PlanToMap(g_selected));
    }

    void Shutdown()
    {
        g_map.reset();
        if (g_graph) {
            ed::DestroyEditor(g_graph);
            g_graph = nullptr;
        }
    }
}
