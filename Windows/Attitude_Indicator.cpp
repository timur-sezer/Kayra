#include "Attitude_Indicator.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <vector>

namespace AttitudeIndicator {
    namespace {
        constexpr float kPi = 3.14159265358979f;
        constexpr float kDegToRad = kPi / 180.0f;
        constexpr int   kCircleSegments = 96;
        constexpr float kVisiblePitchRange = 25.0f;   // degrees from center to the edge of the inner face

        const ImU32 kSky       = IM_COL32( 52, 130, 210, 255);
        const ImU32 kGround    = IM_COL32(140,  90,  45, 255);
        const ImU32 kWhite     = IM_COL32(255, 255, 255, 255);
        const ImU32 kBezel     = IM_COL32( 25,  25,  25, 255);
        const ImU32 kBezelEdge = IM_COL32( 70,  70,  70, 255);
        const ImU32 kAircraft  = IM_COL32(255, 170,   0, 255);
        const ImU32 kOutline   = IM_COL32(  0,   0,   0, 255);

        struct State {
            float pitch = 0.0f;
            float roll  = 0.0f;
            bool  demo  = true;    // animate until real data arrives
            bool  live  = false;
        } g_state;

        inline ImVec2 Add(ImVec2 a, ImVec2 b)    { return ImVec2(a.x + b.x, a.y + b.y); }
        inline ImVec2 Sub(ImVec2 a, ImVec2 b)    { return ImVec2(a.x - b.x, a.y - b.y); }
        inline ImVec2 Mul(ImVec2 a, float s)     { return ImVec2(a.x * s, a.y * s); }
        inline float  Dot(ImVec2 a, ImVec2 b)    { return a.x * b.x + a.y * b.y; }
        inline float  Length(ImVec2 a)           { return std::sqrt(Dot(a, a)); }

        // Point on a circle at `angle` radians measured clockwise from 12 o'clock.
        inline ImVec2 OnCircle(ImVec2 c, float r, float angle)
        {
            return ImVec2(c.x + r * std::sin(angle), c.y - r * std::cos(angle));
        }

        // Clips a convex polygon to the half-plane dot(p - origin, normal) <= 0 (Sutherland–Hodgman).
        std::vector<ImVec2> ClipToHalfPlane(const std::vector<ImVec2>& poly, ImVec2 origin, ImVec2 normal)
        {
            std::vector<ImVec2> out;
            const size_t n = poly.size();
            for (size_t i = 0; i < n; ++i) {
                const ImVec2 a = poly[i];
                const ImVec2 b = poly[(i + 1) % n];
                const float da = Dot(Sub(a, origin), normal);
                const float db = Dot(Sub(b, origin), normal);
                if (da <= 0.0f) out.push_back(a);
                if ((da <= 0.0f) != (db <= 0.0f)) {
                    const float t = da / (da - db);
                    out.push_back(Add(a, Mul(Sub(b, a), t)));
                }
            }
            return out;
        }

        void CenteredText(ImDrawList* dl, ImFont* font, float font_size, ImVec2 center, ImU32 col, const char* text)
        {
            const ImVec2 ts = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, text);
            dl->AddText(font, font_size, ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f), col, text);
        }
    }

    void SetAttitude(float pitch_deg, float roll_deg)
    {
        g_state.pitch = pitch_deg;
        g_state.roll  = roll_deg;
        g_state.demo  = false;
        g_state.live  = true;
    }

    void Draw(float pitch_deg, float roll_deg, float size)
    {
        if (size < 40.0f)
            return;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();

        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 c(origin.x + size * 0.5f, origin.y + size * 0.5f);
        const float R_outer = size * 0.5f;
        const float R       = R_outer * 0.86f;          // inner face (horizon ball)
        const float px_per_deg = (R * 0.8f) / kVisiblePitchRange;
        const float line_w  = std::max(1.0f, size / 200.0f);
        const float font_sz = std::max(10.0f, size / 22.0f);

        pitch_deg = std::clamp(pitch_deg, -90.0f, 90.0f);
        const float roll = roll_deg * kDegToRad;

        // Horizon geometry. Rolling right tilts the horizon counter-clockwise on screen;
        // pitching up moves the horizon down.
        const ImVec2 dir(std::cos(roll), -std::sin(roll));         // along the horizon
        const ImVec2 up(-std::sin(roll), -std::cos(roll));         // towards the sky
        const ImVec2 horizon_center = Sub(c, Mul(up, pitch_deg * px_per_deg));

        // --- Bezel ---
        dl->AddCircleFilled(c, R_outer, kBezel, kCircleSegments);
        dl->AddCircle(c, R_outer - line_w, kBezelEdge, kCircleSegments, line_w * 2.0f);

        // --- Sky (full disc) and ground (disc clipped below the horizon line) ---
        dl->AddCircleFilled(c, R, kSky, kCircleSegments);
        std::vector<ImVec2> disc(kCircleSegments);
        for (int i = 0; i < kCircleSegments; ++i)
            disc[i] = OnCircle(c, R, 2.0f * kPi * i / kCircleSegments);
        std::vector<ImVec2> ground = ClipToHalfPlane(disc, horizon_center, up);
        if (ground.size() >= 3)
            dl->AddConvexPolyFilled(ground.data(), (int)ground.size(), kGround);

        // Horizon line, clipped to the disc.
        {
            const ImVec2 rel = Sub(horizon_center, c);
            const float d = Dot(rel, up);                 // distance of the line from the center
            if (std::fabs(d) < R) {
                const float half = std::sqrt(R * R - d * d);
                const ImVec2 mid = Add(c, Mul(up, d));
                dl->AddLine(Sub(mid, Mul(dir, half)), Add(mid, Mul(dir, half)), kWhite, line_w * 1.5f);
            }
        }

        // --- Pitch ladder (every 5°, labels every 10°) ---
        for (int p = -90; p <= 90; p += 5) {
            if (p == 0) continue;
            const ImVec2 mid = Add(horizon_center, Mul(up, p * px_per_deg));
            if (Length(Sub(mid, c)) > R * 0.72f) continue;     // keep the ladder inside the face

            const bool major = (p % 10 == 0);
            const float half = (major ? R * 0.22f : R * 0.11f);
            const ImVec2 a = Sub(mid, Mul(dir, half));
            const ImVec2 b = Add(mid, Mul(dir, half));
            dl->AddLine(a, b, kWhite, line_w);

            if (major) {
                char label[8];
                snprintf(label, sizeof(label), "%d", std::abs(p));
                const float gap = font_sz * 1.0f;
                CenteredText(dl, font, font_sz, Sub(a, Mul(dir, gap)), kWhite, label);
                CenteredText(dl, font, font_sz, Add(b, Mul(dir, gap)), kWhite, label);
            }
        }

        // --- Roll scale on the bezel (fixed): 0, ±10, ±20, ±30, ±45, ±60 ---
        const float tick_r = R_outer * 0.97f;
        const int ticks[] = { -60, -45, -30, -20, -10, 10, 20, 30, 45, 60 };
        for (int t : ticks) {
            const bool major = (t % 30 == 0);
            const float len = major ? R_outer * 0.12f : R_outer * 0.07f;
            const float a = t * kDegToRad;
            dl->AddLine(OnCircle(c, tick_r, a), OnCircle(c, tick_r - len, a), kWhite, major ? line_w * 2.0f : line_w);
        }
        // Zero mark: inverted triangle at the top.
        {
            const float tr = R_outer * 0.97f;
            const float tw = R_outer * 0.06f;
            const float th = R_outer * 0.10f;
            dl->AddTriangleFilled(ImVec2(c.x - tw, c.y - tr), ImVec2(c.x + tw, c.y - tr), ImVec2(c.x, c.y - tr + th), kWhite);
        }

        // --- Bank pointer (rotates with the horizon) ---
        {
            const float a = -roll;
            const float tip_r  = R * 0.98f;
            const float base_r = R * 0.86f;
            const float spread = 0.06f;
            const ImVec2 tip = OnCircle(c, tip_r, a);
            const ImVec2 l   = OnCircle(c, base_r, a - spread);
            const ImVec2 r   = OnCircle(c, base_r, a + spread);
            dl->AddTriangleFilled(tip, r, l, kAircraft);
            dl->AddTriangle(tip, r, l, kOutline, line_w);
        }

        // --- Fixed aircraft symbol ---
        {
            const float wing   = R * 0.42f;
            const float gap    = R * 0.10f;
            const float thick  = std::max(3.0f, R * 0.035f);
            const float drop   = R * 0.07f;
            // Left wing + drop
            dl->AddRectFilled(ImVec2(c.x - wing, c.y - thick * 0.5f), ImVec2(c.x - gap, c.y + thick * 0.5f), kAircraft);
            dl->AddRectFilled(ImVec2(c.x - gap - thick, c.y), ImVec2(c.x - gap, c.y + drop), kAircraft);
            // Right wing + drop
            dl->AddRectFilled(ImVec2(c.x + gap, c.y - thick * 0.5f), ImVec2(c.x + wing, c.y + thick * 0.5f), kAircraft);
            dl->AddRectFilled(ImVec2(c.x + gap, c.y), ImVec2(c.x + gap + thick, c.y + drop), kAircraft);
            // Center dot
            dl->AddCircleFilled(c, thick * 0.8f, kAircraft, 16);
            dl->AddCircle(c, thick * 0.8f, kOutline, 16, 1.0f);
        }

        // Inner face rim
        dl->AddCircle(c, R, kOutline, kCircleSegments, line_w * 2.0f);

        // Reserve the space so ImGui layout knows about it.
        ImGui::Dummy(ImVec2(size, size));
    }

    void ShowWindow(bool* p_open)
    {
        if (p_open && !*p_open)
            return;

        if (ImGui::Begin(kWindowTitle, p_open)) {
            if (g_state.demo) {
                const float t = (float)ImGui::GetTime();
                g_state.pitch = 10.0f * std::sin(t * 0.7f);
                g_state.roll  = 30.0f * std::sin(t * 0.45f);
            }

            // Right-click: manual test controls (until telemetry is wired in).
            if (ImGui::BeginPopupContextWindow("AttitudeTest")) {
                ImGui::TextDisabled(g_state.live ? "Live telemetry" : "Test input");
                ImGui::Checkbox("Demo motion", &g_state.demo);
                ImGui::SetNextItemWidth(200);
                if (ImGui::SliderFloat("Pitch", &g_state.pitch, -90.0f, 90.0f, "%.1f deg")) g_state.demo = false;
                ImGui::SetNextItemWidth(200);
                if (ImGui::SliderFloat("Roll", &g_state.roll, -180.0f, 180.0f, "%.1f deg")) g_state.demo = false;
                if (ImGui::Button("Level")) { g_state.pitch = 0.0f; g_state.roll = 0.0f; g_state.demo = false; }
                ImGui::EndPopup();
            }

            const float text_h = ImGui::GetTextLineHeightWithSpacing();
            const ImVec2 avail = ImGui::GetContentRegionAvail();
            const float size = std::min(avail.x, avail.y - text_h);

            if (size > 0.0f) {
                // Center the dial horizontally.
                const float pad_x = (avail.x - size) * 0.5f;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + pad_x);
                Draw(g_state.pitch, g_state.roll, size);

                char readout[64];
                snprintf(readout, sizeof(readout), "Pitch %+5.1f   Roll %+6.1f", g_state.pitch, g_state.roll);
                const float tw = ImGui::CalcTextSize(readout).x;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(0.0f, (avail.x - tw) * 0.5f));
                ImGui::TextUnformatted(readout);
            }
        }
        ImGui::End();
    }
}
