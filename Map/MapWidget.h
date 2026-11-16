#pragma once

// An ImGui widget that draws a pannable / zoomable world map (Web Mercator slippy tiles).
//   - drag with the left mouse button to pan
//   - mouse wheel to zoom around the cursor, double-click to zoom in
//   - left click (without dragging) drops a pin and reports its coordinates
//
// Overlays: vehicle (heading arrow + flown trail), home position, mission path with the
// next waypoint highlighted and a line from the vehicle to it.
//
// Based on https://github.com/timur-sezer/Opengl-WorldMap

#include "TileManager.h"
#include "imgui.h"

#include <memory>
#include <string>
#include <vector>

struct MapWaypoint
{
    double lat = 0.0, lon = 0.0;  // degrees
    float alt = 0.0f;             // metres, as stored in the mission item
    int seq = 0;                  // mission item sequence number
    int label = 0;                // number drawn on the marker
};

// What the user did with the map during the last draw().
struct MapEvents
{
    int clickedWaypoint = -1;      // index into mission() clicked without dragging
    int draggedWaypoint = -1;      // index being dragged this frame (edit mode only)
    bool dragEnded = false;        // a waypoint drag finished this frame
    double dragLat = 0.0, dragLon = 0.0;
    bool clickedEmpty = false;     // click (no drag) on the map away from waypoints
    double clickLat = 0.0, clickLon = 0.0;
};

class MapWidget
{
public:
    explicit MapWidget(std::string cacheRoot);

    // Draws the map. A size of 0 on an axis fills the remaining content region.
    void draw(const char* id, ImVec2 size = ImVec2(0, 0));

    static int sourceCount();
    static const TileSource& sourceAt(int index);
    void setSource(int index);
    int sourceIndex() const { return sourceIndex_; }

    void setCenter(double latDeg, double lonDeg);
    double centerLat() const;
    double centerLon() const;
    void setZoom(double zoom);
    double zoom() const { return zoom_; }

    // Vehicle marker; heading in degrees clockwise from north. Every call also extends the trail.
    void setVehicle(double latDeg, double lonDeg, float headingDeg);
    void clearVehicle() { hasVehicle_ = false; }
    bool hasVehicle() const { return hasVehicle_; }
    void clearTrail() { trail_.clear(); }

    // Keep the map centered on the vehicle. Dragging the map turns this off.
    void setFollowVehicle(bool follow) { follow_ = follow; }
    bool followVehicle() const { return follow_; }

    void setHome(double latDeg, double lonDeg);
    void clearHome() { hasHome_ = false; }

    // Mission waypoints in flight order.
    void setMission(std::vector<MapWaypoint> waypoints);
    const std::vector<MapWaypoint>& mission() const { return mission_; }
    // Index into mission() of the waypoint the vehicle is flying to.
    // -1: unknown (no highlight), mission().size(): mission finished.
    void setNextWaypoint(int index) { next_ = index; }

    // Edit mode: waypoints can be dragged, and clicks on empty map are reported instead of dropping a pin.
    void setEditable(bool editable) { editable_ = editable; }
    bool editable() const { return editable_; }
    void setSelectedWaypoint(int index) { selected_ = index; }  // index into mission(), -1 none
    void setShowTrail(bool show) { showTrail_ = show; }
    const MapEvents& events() const { return events_; }

    // Zooms and centers so the mission, vehicle and home all fit (done on the next draw()).
    void fitAll() { fitPending_ = true; }

    // Point picked by left-clicking the map (a click without dragging).
    bool hasPickedPoint() const { return hasPick_; }
    double pickedLat() const;
    double pickedLon() const;
    void clearPickedPoint() { hasPick_ = false; }

private:
    // Screen position of a normalized world point, using the wrapped copy closest to the view center.
    ImVec2 worldToScreen(double x, double y, ImVec2 mid, double ws) const;

    double worldSize() const;  // size of the whole world in screen pixels at the current zoom
    void zoomAround(ImVec2 mouse, ImVec2 mid, double delta);
    int hitTestWaypoint(ImVec2 mouse, ImVec2 mid, double ws) const;
    void screenToLatLon(ImVec2 pos, ImVec2 mid, double ws, double& lat, double& lon) const;
    void applyFit(ImVec2 size);
    void drawTile(ImDrawList* dl, const TileKey& key, ImVec2 a, ImVec2 b);
    void drawPin(ImDrawList* dl, ImVec2 mid, double ws);
    void drawTrail(ImDrawList* dl, ImVec2 mid, double ws);
    void drawMission(ImDrawList* dl, ImVec2 mid, double ws);
    void drawHome(ImDrawList* dl, ImVec2 mid, double ws);
    void drawVehicle(ImDrawList* dl, ImVec2 mid, double ws);

    std::string cacheRoot_;
    std::unique_ptr<TileManager> tiles_;
    int sourceIndex_ = 0;

    // Map center in normalized Web Mercator coordinates, both in [0, 1].
    double cx_ = 0.5, cy_ = 0.5;
    double zoom_ = 3.0;
    bool follow_ = false;
    bool fitPending_ = false;

    bool hasVehicle_ = false;
    double vehicleLat_ = 0.0, vehicleLon_ = 0.0;
    float vehicleHeading_ = 0.0f;
    struct WorldPoint { double x, y; };  // normalized Web Mercator (float is too coarse at high zoom)
    std::vector<WorldPoint> trail_;

    bool hasHome_ = false;
    double homeLat_ = 0.0, homeLon_ = 0.0;

    std::vector<MapWaypoint> mission_;
    int next_ = -1;
    int selected_ = -1;
    bool editable_ = false;
    bool showTrail_ = true;
    int pressedWaypoint_ = -1;   // waypoint under the mouse when the current press started
    MapEvents events_;

    bool hasPick_ = false;
    double pickX_ = 0.0, pickY_ = 0.0;  // normalized Web Mercator
    bool dragged_ = false;               // current mouse press turned into a drag
};
