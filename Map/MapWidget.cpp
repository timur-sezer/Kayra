#include "MapWidget.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace
{
constexpr double kPi = 3.14159265358979323846;
constexpr double kMaxLat = 85.05112878;  // Web Mercator limit
constexpr double kMinZoom = 1.0;
constexpr double kMaxZoom = 20.0;
constexpr double kEarthRadiusM = 6371000.0;

constexpr size_t kMaxTrailPoints = 20000;
constexpr double kTrailMinStep = 5e-8;  // normalized units, about 2 m at the equator

const TileSource kSources[] = {
    {"osm", "OpenStreetMap",
     "https://tile.openstreetmap.org/{z}/{x}/{y}.png",
     "\xC2\xA9 OpenStreetMap contributors", 19},
    {"esri-imagery", "Esri World Imagery (satellite)",
     "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}",
     "Tiles \xC2\xA9 Esri, Maxar, Earthstar Geographics", 19},
};

// Overlay colors
const ImU32 kWhite        = IM_COL32(255, 255, 255, 255);
const ImU32 kShadow       = IM_COL32(0, 0, 0, 150);
const ImU32 kPathColor    = IM_COL32(255, 200, 40, 255);   // legs still to fly
const ImU32 kDoneColor    = IM_COL32(170, 170, 170, 170);  // legs already flown
const ImU32 kNextColor    = IM_COL32(40, 200, 100, 255);   // next waypoint and the line to it
const ImU32 kTrailColor   = IM_COL32(80, 190, 255, 220);
const ImU32 kVehicleColor = IM_COL32(230, 60, 50, 255);
const ImU32 kPinColor     = IM_COL32(40, 120, 230, 255);

double lonToX(double lon) { return (lon + 180.0) / 360.0; }
double xToLon(double x) { return x * 360.0 - 180.0; }

double latToY(double lat)
{
    const double r = std::clamp(lat, -kMaxLat, kMaxLat) * kPi / 180.0;
    return (1.0 - std::log(std::tan(r) + 1.0 / std::cos(r)) / kPi) / 2.0;
}

double yToLat(double y)
{
    return std::atan(std::sinh(kPi * (1.0 - 2.0 * y))) * 180.0 / kPi;
}

// Great-circle distance in metres.
double distanceM(double lat1, double lon1, double lat2, double lon2)
{
    const double p1 = lat1 * kPi / 180.0, p2 = lat2 * kPi / 180.0;
    const double dp = p2 - p1, dl = (lon2 - lon1) * kPi / 180.0;
    const double a = std::sin(dp / 2) * std::sin(dp / 2) + std::cos(p1) * std::cos(p2) * std::sin(dl / 2) * std::sin(dl / 2);
    return 2.0 * kEarthRadiusM * std::asin(std::min(1.0, std::sqrt(a)));
}

ImTextureID toImTexture(GLuint tex) { return (ImTextureID)(intptr_t)tex; }

void label(ImDrawList* dl, ImVec2 pos, const char* text, ImU32 textColor = kWhite)
{
    const ImVec2 ts = ImGui::CalcTextSize(text);
    dl->AddRectFilled(ImVec2(pos.x - 4, pos.y - 2), ImVec2(pos.x + ts.x + 4, pos.y + ts.y + 2),
                      IM_COL32(0, 0, 0, 170), 3.0f);
    dl->AddText(pos, textColor, text);
}

void centeredText(ImDrawList* dl, ImVec2 center, const char* text, ImU32 color)
{
    const ImVec2 ts = ImGui::CalcTextSize(text);
    dl->AddText(ImVec2(std::floor(center.x - ts.x * 0.5f), std::floor(center.y - ts.y * 0.5f)), color, text);
}

void dashedLine(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 color, float thickness, float dash = 10.0f, float gap = 7.0f)
{
    const float dx = b.x - a.x, dy = b.y - a.y;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f)
        return;
    const float ux = dx / len, uy = dy / len;
    for (float t = 0.0f; t < len; t += dash + gap)
    {
        const float e = std::min(t + dash, len);
        dl->AddLine(ImVec2(a.x + ux * t, a.y + uy * t), ImVec2(a.x + ux * e, a.y + uy * e), color, thickness);
    }
}
} // namespace

MapWidget::MapWidget(std::string cacheRoot) : cacheRoot_(std::move(cacheRoot))
{
    setSource(0);
}

int MapWidget::sourceCount() { return int(sizeof(kSources) / sizeof(kSources[0])); }
const TileSource& MapWidget::sourceAt(int index) { return kSources[index]; }

void MapWidget::setSource(int index)
{
    index = std::clamp(index, 0, sourceCount() - 1);
    if (tiles_ && index == sourceIndex_)
        return;
    sourceIndex_ = index;
    tiles_.reset();  // stop old workers before starting new ones
    tiles_ = std::make_unique<TileManager>(kSources[index], cacheRoot_ + "/" + kSources[index].id);
}

void MapWidget::setCenter(double latDeg, double lonDeg)
{
    cx_ = lonToX(lonDeg);
    cy_ = latToY(latDeg);
}

double MapWidget::centerLat() const { return yToLat(cy_); }
double MapWidget::centerLon() const { return xToLon(cx_); }
void MapWidget::setZoom(double zoom) { zoom_ = std::clamp(zoom, kMinZoom, kMaxZoom); }

void MapWidget::setVehicle(double latDeg, double lonDeg, float headingDeg)
{
    hasVehicle_ = true;
    vehicleLat_ = latDeg;
    vehicleLon_ = lonDeg;
    vehicleHeading_ = headingDeg;

    const WorldPoint p{lonToX(lonDeg), latToY(latDeg)};
    if (!trail_.empty())
    {
        const WorldPoint& last = trail_.back();
        if (std::abs(p.x - last.x) < kTrailMinStep && std::abs(p.y - last.y) < kTrailMinStep)
            return;
    }
    if (trail_.size() >= kMaxTrailPoints)
        trail_.erase(trail_.begin(), trail_.begin() + kMaxTrailPoints / 4);
    trail_.push_back(p);
}

void MapWidget::setHome(double latDeg, double lonDeg)
{
    hasHome_ = true;
    homeLat_ = latDeg;
    homeLon_ = lonDeg;
}

void MapWidget::setMission(std::vector<MapWaypoint> waypoints)
{
    mission_ = std::move(waypoints);
    next_ = -1;
}

static float markerRadius() { return std::max(9.0f, ImGui::GetFontSize() * 0.55f); }

double MapWidget::worldSize() const { return 256.0 * std::pow(2.0, zoom_); }

void MapWidget::zoomAround(ImVec2 mouse, ImVec2 mid, double delta)
{
    // Keep the point under the cursor fixed while zooming.
    double ws = worldSize();
    const double mx = cx_ + (mouse.x - mid.x) / ws;
    const double my = cy_ + (mouse.y - mid.y) / ws;
    setZoom(zoom_ + delta);
    ws = worldSize();
    cx_ = mx - (mouse.x - mid.x) / ws;
    cy_ = my - (mouse.y - mid.y) / ws;
}

int MapWidget::hitTestWaypoint(ImVec2 mouse, ImVec2 mid, double ws) const
{
    const float r = markerRadius() + 4.0f;
    // Last drawn is on top, and the selected/next markers are drawn last.
    for (int pass = 0; pass < 2; ++pass)
        for (int i = int(mission_.size()) - 1; i >= 0; --i)
        {
            const bool priority = i == selected_ || i == next_;
            if ((pass == 0) != priority)
                continue;
            const ImVec2 p = worldToScreen(lonToX(mission_[i].lon), latToY(mission_[i].lat), mid, ws);
            const float dx = p.x - mouse.x, dy = p.y - mouse.y;
            if (dx * dx + dy * dy <= r * r)
                return i;
        }
    return -1;
}

void MapWidget::screenToLatLon(ImVec2 pos, ImVec2 mid, double ws, double& lat, double& lon) const
{
    double x = cx_ + (pos.x - mid.x) / ws;
    x -= std::floor(x);
    const double y = std::clamp(cy_ + (pos.y - mid.y) / ws, 0.0, 1.0);
    lat = yToLat(y);
    lon = xToLon(x);
}

void MapWidget::applyFit(ImVec2 size)
{
    double minX = 1e9, maxX = -1e9, minY = 1e9, maxY = -1e9;
    auto add = [&](double lat, double lon) {
        const double x = lonToX(lon), y = latToY(lat);
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minY = std::min(minY, y); maxY = std::max(maxY, y);
    };
    for (const MapWaypoint& w : mission_)
        add(w.lat, w.lon);
    if (hasVehicle_)
        add(vehicleLat_, vehicleLon_);
    if (hasHome_)
        add(homeLat_, homeLon_);
    if (minX > maxX)
        return;  // nothing to fit

    cx_ = (minX + maxX) * 0.5;
    cy_ = (minY + maxY) * 0.5;

    const double margin = 60.0;  // pixels kept free around the content
    const double w = std::max(1.0, double(size.x) - 2 * margin);
    const double h = std::max(1.0, double(size.y) - 2 * margin);
    const double spanX = std::max(maxX - minX, 1e-9), spanY = std::max(maxY - minY, 1e-9);
    const double z = std::log2(std::min(w / (spanX * 256.0), h / (spanY * 256.0)));
    setZoom(std::min(z, 18.0));
}

void MapWidget::draw(const char* id, ImVec2 size)
{
    tiles_->beginFrame();

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (size.x <= 0) size.x = avail.x;
    if (size.y <= 0) size.y = avail.y;
    if (size.x < 1 || size.y < 1)
        return;

    if (fitPending_)
    {
        applyFit(size);
        fitPending_ = false;
    }

    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + size.x, p0.y + size.y);
    const ImVec2 mid(std::floor((p0.x + p1.x) * 0.5f), std::floor((p0.y + p1.y) * 0.5f));

    // --- Input ---
    ImGui::InvisibleButton(id, size, ImGuiButtonFlags_MouseButtonLeft);
    const bool hovered = ImGui::IsItemHovered();
    ImGuiIO& io = ImGui::GetIO();

    events_ = MapEvents();
    {
        const double ws = worldSize();
        const int under = hovered ? hitTestWaypoint(io.MousePos, mid, ws) : -1;
        if (under >= 0)
            ImGui::SetMouseCursor(editable_ ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_Hand);

        // A press that is released without moving past the drag threshold is a click.
        if (ImGui::IsItemActivated())
        {
            dragged_ = false;
            pressedWaypoint_ = under;
        }
        const bool draggingWaypoint = editable_ && pressedWaypoint_ >= 0;
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            dragged_ = true;
            if (!draggingWaypoint)
                follow_ = false;  // the user is looking somewhere else
        }

        if (draggingWaypoint && dragged_ && ImGui::IsItemActive() && pressedWaypoint_ < int(mission_.size()))
        {
            // Move the marker right away; the owner gets the new position through events().
            screenToLatLon(io.MousePos, mid, ws, events_.dragLat, events_.dragLon);
            events_.draggedWaypoint = pressedWaypoint_;
            mission_[pressedWaypoint_].lat = events_.dragLat;
            mission_[pressedWaypoint_].lon = events_.dragLon;
        }

        if (ImGui::IsItemDeactivated())
        {
            if (!dragged_ && pressedWaypoint_ >= 0)
                events_.clickedWaypoint = pressedWaypoint_;
            else if (!dragged_)
            {
                events_.clickedEmpty = true;
                screenToLatLon(io.MousePos, mid, ws, events_.clickLat, events_.clickLon);
                if (!editable_)
                {
                    pickX_ = lonToX(events_.clickLon);
                    pickY_ = latToY(events_.clickLat);
                    hasPick_ = true;
                }
            }
            else if (draggingWaypoint)
            {
                events_.dragEnded = true;
                events_.draggedWaypoint = pressedWaypoint_;
                screenToLatLon(io.MousePos, mid, ws, events_.dragLat, events_.dragLon);
            }
            pressedWaypoint_ = -1;
        }

        if (ImGui::IsItemActive() && !draggingWaypoint && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
        {
            cx_ -= io.MouseDelta.x / ws;
            cy_ -= io.MouseDelta.y / ws;
        }
    }
    if (hovered && io.MouseWheel != 0.0f)
        zoomAround(io.MousePos, mid, io.MouseWheel * 0.25);
    // In edit mode a double-click is two waypoint clicks, so it doesn't zoom.
    if (hovered && !editable_ && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        zoomAround(io.MousePos, mid, 1.0);

    if (follow_ && hasVehicle_)
    {
        cx_ = lonToX(vehicleLon_);
        cy_ = latToY(vehicleLat_);
    }

    cx_ -= std::floor(cx_);  // wrap around the date line
    cy_ = std::clamp(cy_, 0.0, 1.0);

    // --- Tiles ---
    const double ws = worldSize();
    const int tz = std::clamp(int(std::lround(zoom_)), 0, tiles_->source().maxZoom);
    const int n = 1 << tz;

    const double left = cx_ - (mid.x - p0.x) / ws;
    const double right = cx_ + (p1.x - mid.x) / ws;
    const double top = cy_ - (mid.y - p0.y) / ws;
    const double bottom = cy_ + (p1.y - mid.y) / ws;
    const int tx0 = int(std::floor(left * n));
    const int tx1 = int(std::floor(right * n));
    const int ty0 = std::max(0, int(std::floor(top * n)));
    const int ty1 = std::min(n - 1, int(std::floor(bottom * n)));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->PushClipRect(p0, p1, true);
    dl->AddRectFilled(p0, p1, IM_COL32(24, 28, 34, 255));

    auto toScreenX = [&](double x) { return float(std::floor(mid.x + (x - cx_) * ws)); };
    auto toScreenY = [&](double y) { return float(std::floor(mid.y + (y - cy_) * ws)); };

    for (int ty = ty0; ty <= ty1; ++ty)
    {
        for (int tx = tx0; tx <= tx1; ++tx)
        {
            const int wx = ((tx % n) + n) % n;  // horizontal wrap
            const ImVec2 a(toScreenX(double(tx) / n), toScreenY(double(ty) / n));
            const ImVec2 b(toScreenX(double(tx + 1) / n), toScreenY(double(ty + 1) / n));
            drawTile(dl, TileKey{tz, wx, ty}, a, b);
        }
    }

    // --- Overlays (back to front) ---
    if (showTrail_)
        drawTrail(dl, mid, ws);
    drawMission(dl, mid, ws);
    if (hasHome_)
        drawHome(dl, mid, ws);
    if (hasPick_)
        drawPin(dl, mid, ws);
    if (hasVehicle_)
        drawVehicle(dl, mid, ws);

    char buf[128];
    if (hovered)
    {
        double mx = cx_ + (io.MousePos.x - mid.x) / ws;
        mx -= std::floor(mx);
        const double my = std::clamp(cy_ + (io.MousePos.y - mid.y) / ws, 0.0, 1.0);
        snprintf(buf, sizeof(buf), "%.6f, %.6f   z %.2f", yToLat(my), xToLon(mx), zoom_);
        label(dl, ImVec2(p0.x + 8, p1.y - ImGui::GetTextLineHeight() - 6), buf);
    }

    const char* attribution = tiles_->source().attribution;
    const float aw = ImGui::CalcTextSize(attribution).x;
    label(dl, ImVec2(p1.x - aw - 8, p1.y - ImGui::GetTextLineHeight() - 6), attribution);

    dl->PopClipRect();
}

void MapWidget::drawTile(ImDrawList* dl, const TileKey& key, ImVec2 a, ImVec2 b)
{
    if (GLuint tex = tiles_->getTexture(key, true))
    {
        dl->AddImage(toImTexture(tex), a, b);
        return;
    }

    // Not loaded yet: stretch the matching part of an already loaded lower-zoom tile.
    for (int d = 1; d <= 8 && key.z - d >= 0; ++d)
    {
        const TileKey parent{key.z - d, key.x >> d, key.y >> d};
        const GLuint tex = tiles_->getTexture(parent, false);
        if (!tex)
            continue;
        const float s = 1.0f / float(1 << d);
        const ImVec2 uv0(float(key.x - (parent.x << d)) * s, float(key.y - (parent.y << d)) * s);
        dl->AddImage(toImTexture(tex), a, b, uv0, ImVec2(uv0.x + s, uv0.y + s));
        return;
    }
}

double MapWidget::pickedLat() const { return yToLat(pickY_); }
double MapWidget::pickedLon() const { return xToLon(pickX_); }

ImVec2 MapWidget::worldToScreen(double x, double y, ImVec2 mid, double ws) const
{
    double dx = x - cx_;
    dx -= std::round(dx);  // the copy closest to the view center
    return ImVec2(float(mid.x + dx * ws), float(mid.y + (y - cy_) * ws));
}

void MapWidget::drawPin(ImDrawList* dl, ImVec2 mid, double ws)
{
    const ImVec2 p = worldToScreen(pickX_, pickY_, mid, ws);

    // Teardrop pin whose tip is exactly on the picked point
    const ImVec2 head(p.x, p.y - 22.0f);
    dl->AddTriangleFilled(ImVec2(p.x - 6.5f, head.y + 4.0f), ImVec2(p.x + 6.5f, head.y + 4.0f), p, kPinColor);
    dl->AddCircleFilled(head, 8.0f, kPinColor);
    dl->AddCircleFilled(head, 3.0f, kWhite);
    dl->AddCircle(p, 3.0f, IM_COL32(255, 255, 255, 220), 0, 1.5f);

    char buf[64];
    snprintf(buf, sizeof(buf), "%.6f, %.6f", pickedLat(), pickedLon());
    label(dl, ImVec2(p.x + 14.0f, head.y - ImGui::GetTextLineHeight() * 0.5f), buf);
}

void MapWidget::drawTrail(ImDrawList* dl, ImVec2 mid, double ws)
{
    if (trail_.size() < 2)
        return;
    std::vector<ImVec2> pts;
    pts.reserve(trail_.size() + 1);
    for (const WorldPoint& p : trail_)
        pts.push_back(worldToScreen(p.x, p.y, mid, ws));
    if (hasVehicle_)  // connect the last stored point to the live position
        pts.push_back(worldToScreen(lonToX(vehicleLon_), latToY(vehicleLat_), mid, ws));
    dl->AddPolyline(pts.data(), int(pts.size()), kTrailColor, ImDrawFlags_None, 2.0f);
}

void MapWidget::drawMission(ImDrawList* dl, ImVec2 mid, double ws)
{
    const int n = int(mission_.size());
    if (n == 0)
        return;

    std::vector<ImVec2> sp(n);
    for (int i = 0; i < n; ++i)
        sp[i] = worldToScreen(lonToX(mission_[i].lon), latToY(mission_[i].lat), mid, ws);

    const bool known = next_ >= 0;
    auto reached = [&](int i) { return known && i < next_; };  // waypoint i already passed

    // Legs: flown ones grey, remaining ones yellow with a dark outline.
    for (int i = 0; i + 1 < n; ++i)
    {
        if (reached(i + 1))
            dl->AddLine(sp[i], sp[i + 1], kDoneColor, 2.0f);
        else
        {
            dl->AddLine(sp[i], sp[i + 1], kShadow, 5.0f);
            dl->AddLine(sp[i], sp[i + 1], kPathColor, 3.0f);
        }
    }

    // Vehicle -> next waypoint
    const bool hasNext = known && next_ < n;
    if (hasNext && hasVehicle_)
    {
        const ImVec2 v = worldToScreen(lonToX(vehicleLon_), latToY(vehicleLat_), mid, ws);
        dashedLine(dl, v, sp[next_], kShadow, 4.5f);
        dashedLine(dl, v, sp[next_], kNextColor, 2.5f);
    }

    // Markers. The selected and next ones are drawn last so they are on top.
    const float r = markerRadius();
    char num[16];
    auto marker = [&](int i) {
        snprintf(num, sizeof(num), "%d", mission_[i].label);
        if (i == selected_)
            dl->AddCircle(sp[i], r + 6.0f, IM_COL32(80, 200, 255, 255), 0, 3.0f);
        if (i == next_)
        {
            // Pulsing ring around the next waypoint
            const float phase = float(std::fmod(ImGui::GetTime(), 1.5) / 1.5);
            dl->AddCircle(sp[i], r + 4.0f + 12.0f * phase, IM_COL32(40, 200, 100, int(220 * (1.0f - phase))), 0, 2.5f);
            dl->AddCircleFilled(sp[i], r + 2.0f, kNextColor);
            dl->AddCircle(sp[i], r + 2.0f, kWhite, 0, 2.0f);
            centeredText(dl, sp[i], num, kWhite);
        }
        else if (reached(i))
        {
            dl->AddCircleFilled(sp[i], r, IM_COL32(90, 90, 90, 220));
            dl->AddCircle(sp[i], r, IM_COL32(200, 200, 200, 200), 0, 1.5f);
            centeredText(dl, sp[i], num, IM_COL32(220, 220, 220, 255));
        }
        else
        {
            dl->AddCircleFilled(sp[i], r, IM_COL32(25, 25, 25, 235));
            dl->AddCircle(sp[i], r, kPathColor, 0, 2.0f);
            centeredText(dl, sp[i], num, kPathColor);
        }
    };
    for (int i = 0; i < n; ++i)
        if (i != next_ && i != selected_)
            marker(i);
    if (selected_ >= 0 && selected_ < n && selected_ != next_)
        marker(selected_);

    if (hasNext)
    {
        marker(next_);
        char buf[64];
        if (hasVehicle_)
        {
            const double d = distanceM(vehicleLat_, vehicleLon_, mission_[next_].lat, mission_[next_].lon);
            if (d >= 1000.0) snprintf(buf, sizeof(buf), "NEXT  %.2f km", d / 1000.0);
            else             snprintf(buf, sizeof(buf), "NEXT  %.0f m", d);
        }
        else
            snprintf(buf, sizeof(buf), "NEXT");
        // Label right of the marker, or left of it when it would run off the map.
        const float tw = ImGui::CalcTextSize(buf).x;
        float lx = sp[next_].x + r + 8.0f;
        if (lx + tw + 4.0f > dl->GetClipRectMax().x)
            lx = sp[next_].x - r - 8.0f - tw;
        label(dl, ImVec2(lx, sp[next_].y - ImGui::GetTextLineHeight() * 0.5f), buf, kNextColor);
    }
}

void MapWidget::drawHome(ImDrawList* dl, ImVec2 mid, double ws)
{
    const ImVec2 c = worldToScreen(lonToX(homeLon_), latToY(homeLat_), mid, ws);
    const float r = markerRadius();
    dl->AddCircleFilled(c, r, IM_COL32(150, 70, 210, 240));
    dl->AddCircle(c, r, kWhite, 0, 2.0f);
    centeredText(dl, c, "H", kWhite);
}

void MapWidget::drawVehicle(ImDrawList* dl, ImVec2 mid, double ws)
{
    const ImVec2 c = worldToScreen(lonToX(vehicleLon_), latToY(vehicleLat_), mid, ws);

    const float h = vehicleHeading_ * float(kPi / 180.0);
    const float cs = std::cos(h), sn = std::sin(h);
    auto pt = [&](float x, float y) { return ImVec2(c.x + x * cs - y * sn, c.y + x * sn + y * cs); };

    const ImVec2 tip = pt(0, -16), right = pt(10, 11), notch = pt(0, 5), left = pt(-10, 11);
    dl->AddTriangleFilled(tip, right, notch, kVehicleColor);
    dl->AddTriangleFilled(tip, notch, left, kVehicleColor);
    const ImVec2 outline[] = {tip, right, notch, left};
    dl->AddPolyline(outline, 4, kWhite, ImDrawFlags_Closed, 2.0f);
}
