#pragma once

// KAYRA design system: color tokens, fonts, ImGui style and a few shared widgets.
// Every window draws through these so the whole app reads as one product.

#include "imgui.h"

namespace Theme {
    // ---- Color tokens (filled for the active theme) ----
    struct Palette {
        ImVec4 bg;          // app background, dock gaps, tab strips
        ImVec4 surface;     // window bodies
        ImVec4 raised;      // inputs, buttons, table headers
        ImVec4 hover;       // hovered controls
        ImVec4 border;      // hairlines
        ImVec4 text;        // primary text
        ImVec4 text_dim;    // labels, secondary text
        ImVec4 text_faint;  // hints, disabled
        ImVec4 accent;      // the one brand/interactive color
        ImVec4 on_accent;   // text on accent fills
        ImVec4 good, warn, bad;
        bool   dark = true;
    };
    const Palette& P();
    ImU32 Col(const ImVec4& c, float alpha_mul = 1.0f);

    // ---- Fonts ----
    enum class Font { Body, Small, Medium, Heading, Display, Count };
    // Loads Open Sans in several weights/sizes. `dpi` is the framebuffer/window ratio (2 on Retina):
    // glyphs are rasterized at that scale so text stays sharp.
    void LoadFonts(const char* assets_dir, float dpi);
    ImFont* Get(Font f);
    struct FontScope {  // RAII PushFont/PopFont
        explicit FontScope(Font f) { ImGui::PushFont(Get(f)); }
        ~FontScope() { ImGui::PopFont(); }
    };

    // ---- Style ----
    enum Variant : int { KayraDark = 0, KayraLight, ImGuiDark, ImGuiClassic, VariantCount };
    // Applies colors and metrics. `accent` overrides the theme accent when non-null.
    void Apply(int variant, float ui_scale, float rounding, const float* accent);
    float Scale();  // the user's UI scale (layout code multiplies fixed sizes by this)

    // Makes the button at the left of every docked window's tab bar hide that tab bar with one
    // click (ImGui would open a menu). The small triangle left in the corner shows it again.
    void InstallTabBarButton();

    // ---- Widgets ----
    bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
    bool DangerButton(const char* label, const ImVec2& size = ImVec2(0, 0));
    void Pill(const char* text, const ImVec4& color);          // small rounded status badge with a dot
    void Section(const char* label);                           // quiet left-aligned section label + hairline
    void Dot(const ImVec4& color);                             // inline status dot (no text)
}
