#include "Theme.h"

#include "imgui_internal.h"  // ImLerp

#include <algorithm>
#include <cstdio>
#include <string>

namespace Theme {
    namespace {
        Palette g_palette;
        ImFont* g_fonts[int(Font::Count)] = {};
        float   g_dpi = 1.0f;
        float   g_scale = 1.0f;

        ImVec4 Rgb(int r, int g, int b, float a = 1.0f) { return ImVec4(r / 255.0f, g / 255.0f, b / 255.0f, a); }
        ImVec4 WithAlpha(ImVec4 c, float a) { c.w = a; return c; }
        ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t) { return ImLerp(a, b, t); }

        Palette DarkPalette()
        {
            Palette p;
            p.bg         = Rgb(13, 15, 19);
            p.surface    = Rgb(21, 24, 30);
            p.raised     = Rgb(30, 34, 42);
            p.hover      = Rgb(40, 45, 56);
            p.border     = Rgb(43, 48, 58);
            p.text       = Rgb(232, 234, 237);
            p.text_dim   = Rgb(150, 157, 168);
            p.text_faint = Rgb(96, 103, 115);
            p.accent     = Rgb(10, 132, 255);   // Apple systemBlue (dark)
            p.on_accent  = Rgb(255, 255, 255);
            p.good       = Rgb(48, 209, 88);
            p.warn       = Rgb(255, 176, 32);
            p.bad        = Rgb(255, 69, 58);
            p.dark       = true;
            return p;
        }

        Palette LightPalette()
        {
            Palette p;
            p.bg         = Rgb(236, 238, 242);
            p.surface    = Rgb(250, 250, 252);
            p.raised     = Rgb(240, 241, 245);
            p.hover      = Rgb(228, 231, 236);
            p.border     = Rgb(214, 218, 225);
            p.text       = Rgb(28, 30, 33);
            p.text_dim   = Rgb(92, 99, 110);
            p.text_faint = Rgb(150, 156, 166);
            p.accent     = Rgb(0, 122, 255);    // Apple systemBlue (light)
            p.on_accent  = Rgb(255, 255, 255);
            p.good       = Rgb(36, 138, 61);
            p.warn       = Rgb(196, 110, 0);
            p.bad        = Rgb(215, 0, 21);
            p.dark       = false;
            return p;
        }

        void ApplyColors(const Palette& p)
        {
            ImVec4* c = ImGui::GetStyle().Colors;
            const ImVec4 none(0, 0, 0, 0);
            const ImVec4 accent_hover = Mix(p.accent, p.dark ? ImVec4(1, 1, 1, 1) : ImVec4(0, 0, 0, 1), 0.12f);

            c[ImGuiCol_Text]                  = p.text;
            c[ImGuiCol_TextDisabled]          = p.text_dim;
            c[ImGuiCol_WindowBg]              = p.surface;
            c[ImGuiCol_ChildBg]               = none;
            c[ImGuiCol_PopupBg]               = WithAlpha(p.dark ? p.raised : p.surface, 0.98f);
            c[ImGuiCol_Border]                = p.border;
            c[ImGuiCol_BorderShadow]          = none;
            c[ImGuiCol_FrameBg]               = p.raised;
            c[ImGuiCol_FrameBgHovered]        = p.hover;
            c[ImGuiCol_FrameBgActive]         = Mix(p.hover, p.accent, 0.15f);
            c[ImGuiCol_TitleBg]               = p.bg;
            c[ImGuiCol_TitleBgActive]         = p.bg;
            c[ImGuiCol_TitleBgCollapsed]      = p.bg;
            c[ImGuiCol_MenuBarBg]             = p.bg;
            c[ImGuiCol_ScrollbarBg]           = none;
            c[ImGuiCol_ScrollbarGrab]         = p.hover;
            c[ImGuiCol_ScrollbarGrabHovered]  = Mix(p.hover, p.text_faint, 0.5f);
            c[ImGuiCol_ScrollbarGrabActive]   = p.text_faint;
            c[ImGuiCol_CheckMark]             = p.accent;
            c[ImGuiCol_SliderGrab]            = p.accent;
            c[ImGuiCol_SliderGrabActive]      = accent_hover;
            c[ImGuiCol_Button]                = p.raised;
            c[ImGuiCol_ButtonHovered]         = p.hover;
            c[ImGuiCol_ButtonActive]          = Mix(p.hover, p.accent, 0.25f);
            c[ImGuiCol_Header]                = WithAlpha(p.accent, 0.16f);
            c[ImGuiCol_HeaderHovered]         = WithAlpha(p.accent, 0.10f);
            c[ImGuiCol_HeaderActive]          = WithAlpha(p.accent, 0.24f);
            c[ImGuiCol_Separator]             = p.border;
            c[ImGuiCol_SeparatorHovered]      = WithAlpha(p.accent, 0.6f);
            c[ImGuiCol_SeparatorActive]       = p.accent;
            c[ImGuiCol_ResizeGrip]            = none;
            c[ImGuiCol_ResizeGripHovered]     = WithAlpha(p.accent, 0.5f);
            c[ImGuiCol_ResizeGripActive]      = p.accent;
            c[ImGuiCol_Tab]                   = p.bg;
            c[ImGuiCol_TabHovered]            = p.raised;
            c[ImGuiCol_TabActive]             = p.surface;
            c[ImGuiCol_TabUnfocused]          = p.bg;
            c[ImGuiCol_TabUnfocusedActive]    = p.surface;
            c[ImGuiCol_DockingPreview]        = WithAlpha(p.accent, 0.35f);
            c[ImGuiCol_DockingEmptyBg]        = p.bg;
            c[ImGuiCol_PlotLines]             = p.accent;
            c[ImGuiCol_PlotLinesHovered]      = accent_hover;
            c[ImGuiCol_PlotHistogram]         = p.accent;
            c[ImGuiCol_PlotHistogramHovered]  = accent_hover;
            c[ImGuiCol_TableHeaderBg]         = p.raised;
            c[ImGuiCol_TableBorderStrong]     = p.border;
            c[ImGuiCol_TableBorderLight]      = WithAlpha(p.border, 0.6f);
            c[ImGuiCol_TableRowBg]            = none;
            c[ImGuiCol_TableRowBgAlt]         = p.dark ? ImVec4(1, 1, 1, 0.025f) : ImVec4(0, 0, 0, 0.025f);
            c[ImGuiCol_TextSelectedBg]        = WithAlpha(p.accent, 0.35f);
            c[ImGuiCol_DragDropTarget]        = p.accent;
            c[ImGuiCol_NavHighlight]          = p.accent;
            c[ImGuiCol_NavWindowingHighlight] = WithAlpha(p.text, 0.7f);
            c[ImGuiCol_NavWindowingDimBg]     = ImVec4(0, 0, 0, 0.4f);
            c[ImGuiCol_ModalWindowDimBg]      = ImVec4(0, 0, 0, p.dark ? 0.55f : 0.30f);
        }

        void ApplyMetrics(float rounding)
        {
            ImGuiStyle& s = ImGui::GetStyle();
            // 4/8-pt rhythm
            s.WindowPadding       = ImVec2(12, 12);
            s.FramePadding        = ImVec2(10, 6);
            s.CellPadding         = ImVec2(8, 5);
            s.ItemSpacing         = ImVec2(8, 8);
            s.ItemInnerSpacing    = ImVec2(6, 6);
            s.TouchExtraPadding   = ImVec2(0, 0);
            s.IndentSpacing       = 16;
            s.ScrollbarSize       = 10;
            s.GrabMinSize         = 10;
            // Hairlines only where they separate things
            s.WindowBorderSize    = 0;
            s.ChildBorderSize     = 1;
            s.PopupBorderSize     = 1;
            s.FrameBorderSize     = 0;
            s.TabBorderSize       = 0;
            // Shape
            s.WindowRounding      = rounding;
            s.ChildRounding       = rounding;
            s.FrameRounding       = std::min(rounding, 6.0f);
            s.PopupRounding       = rounding;
            s.ScrollbarRounding   = rounding;
            s.GrabRounding        = std::min(rounding, 4.0f);
            s.TabRounding         = std::min(rounding, 6.0f);
            // Alignment: titles and section labels read left to right
            s.WindowTitleAlign    = ImVec2(0.0f, 0.5f);
            s.WindowMenuButtonPosition = ImGuiDir_Left;  // tab-bar button: hides the tab bar (see InstallTabBarButton)
            s.ButtonTextAlign     = ImVec2(0.5f, 0.5f);
            s.SeparatorTextAlign  = ImVec2(0.0f, 0.5f);
            s.SeparatorTextBorderSize = 1.0f;
            s.SeparatorTextPadding = ImVec2(0.0f, 6.0f);
            s.AntiAliasedLines    = true;
            s.AntiAliasedFill     = true;
        }
    }

    const Palette& P() { return g_palette; }

    ImU32 Col(const ImVec4& c, float alpha_mul)
    {
        return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, c.w * alpha_mul));
    }

    void LoadFonts(const char* assets_dir, float dpi)
    {
        ImGuiIO& io = ImGui::GetIO();
        g_dpi = std::max(1.0f, dpi);

        // Latin + Latin Extended-A (Turkish), punctuation, arrows
        static const ImWchar ranges[] = {
            0x0020, 0x00FF, 0x0100, 0x017F, 0x2010, 0x2027, 0x2190, 0x21FF, 0x2212, 0x2212, 0,
        };

        struct Spec { Font id; const char* file; float size; };
        const Spec specs[] = {
            {Font::Body,    "OpenSans-Regular.ttf",  15.0f},  // first = default font
            {Font::Small,   "OpenSans-Medium.ttf",   12.5f},
            {Font::Medium,  "OpenSans-Medium.ttf",   15.0f},
            {Font::Heading, "OpenSans-SemiBold.ttf", 17.0f},
            {Font::Display, "OpenSans-SemiBold.ttf", 26.0f},
        };

        for (const Spec& sp : specs) {
            ImFontConfig cfg;
            cfg.OversampleH = 2;
            cfg.OversampleV = 1;
            cfg.PixelSnapH = false;
            const std::string path = std::string(assets_dir) + "/fonts/Open_Sans/static/" + sp.file;
            ImFont* f = io.Fonts->AddFontFromFileTTF(path.c_str(), sp.size * g_dpi, &cfg, ranges);
            if (!f) {
                fprintf(stderr, "KAYRA: could not load %s\n", path.c_str());
                ImFontConfig def;
                def.SizePixels = sp.size * g_dpi;
                f = io.Fonts->AddFontDefault(&def);
            }
            g_fonts[int(sp.id)] = f;
        }
        io.FontDefault = g_fonts[int(Font::Body)];
        io.FontGlobalScale = 1.0f / g_dpi;
    }

    ImFont* Get(Font f) { return g_fonts[int(f)] ? g_fonts[int(f)] : ImGui::GetFont(); }

    float Scale() { return g_scale; }

    void InstallTabBarButton()
    {
        GImGui->DockNodeWindowMenuHandler = [](ImGuiContext*, ImGuiDockNode* node, ImGuiTabBar*) {
            node->WantHiddenTabBarToggle = true;
            ImGui::CloseCurrentPopup();
        };
    }

    void Apply(int variant, float ui_scale, float rounding, const float* accent)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        style = ImGuiStyle();
        ApplyMetrics(rounding);

        switch (variant) {
            case KayraLight:
                g_palette = LightPalette();
                ApplyColors(g_palette);
                break;
            case ImGuiDark:
                g_palette = DarkPalette();
                ImGui::StyleColorsDark();
                break;
            case ImGuiClassic:
                g_palette = DarkPalette();
                ImGui::StyleColorsClassic();
                break;
            default:
                g_palette = DarkPalette();
                ApplyColors(g_palette);
                break;
        }

        if (accent) {
            g_palette.accent = ImVec4(accent[0], accent[1], accent[2], 1.0f);
            if (variant == KayraDark || variant == KayraLight)
                ApplyColors(g_palette);
        }

        g_scale = ui_scale;
        style.ScaleAllSizes(ui_scale);
        ImGui::GetIO().FontGlobalScale = ui_scale / g_dpi;
    }

    // ---- Widgets ----

    bool PrimaryButton(const char* label, const ImVec2& size)
    {
        const Palette& p = g_palette;
        const ImVec4 hover = Mix(p.accent, p.dark ? ImVec4(1, 1, 1, 1) : ImVec4(0, 0, 0, 1), 0.12f);
        ImGui::PushStyleColor(ImGuiCol_Button, p.accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, Mix(p.accent, ImVec4(0, 0, 0, 1), 0.15f));
        ImGui::PushStyleColor(ImGuiCol_Text, p.on_accent);
        ImGui::PushFont(Get(Font::Medium));
        const bool pressed = ImGui::Button(label, size);
        ImGui::PopFont();
        ImGui::PopStyleColor(4);
        return pressed;
    }

    bool DangerButton(const char* label, const ImVec2& size)
    {
        const Palette& p = g_palette;
        // Quiet until hovered: destructive actions shouldn't outshine the primary one.
        ImGui::PushStyleColor(ImGuiCol_Button, WithAlpha(p.bad, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, WithAlpha(p.bad, 0.14f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, WithAlpha(p.bad, 0.24f));
        ImGui::PushStyleColor(ImGuiCol_Border, WithAlpha(p.bad, 0.35f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, p.bad);
        const bool pressed = ImGui::Button(label, size);
        ImGui::PopStyleVar();
        ImGui::PopStyleColor(5);
        return pressed;
    }

    void Pill(const char* text, const ImVec4& color)
    {
        ImGui::PushFont(Get(Font::Small));
        const ImVec2 ts = ImGui::CalcTextSize(text);
        const float pad_x = 8.0f * g_scale, pad_y = 3.0f * g_scale, dot = 3.0f * g_scale;
        const ImVec2 size(ts.x + pad_x * 2.0f + dot * 2.0f + 6.0f * g_scale, ts.y + pad_y * 2.0f);

        // Vertically center on the current line
        const float line_h = ImGui::GetFrameHeight();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        pos.y += std::max(0.0f, (line_h - size.y) * 0.5f);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), Col(color, 0.14f), size.y * 0.5f);
        dl->AddCircleFilled(ImVec2(pos.x + pad_x + dot, pos.y + size.y * 0.5f), dot, Col(color));
        dl->AddText(ImVec2(pos.x + pad_x + dot * 2.0f + 6.0f * g_scale, pos.y + pad_y), Col(color), text);
        ImGui::PopFont();
        ImGui::Dummy(ImVec2(size.x, std::max(size.y, line_h)));
    }

    void Section(const char* label)
    {
        ImGui::Spacing();
        ImGui::PushFont(Get(Font::Small));
        ImGui::PushStyleColor(ImGuiCol_Text, g_palette.text_dim);
        ImGui::SeparatorText(label);
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }

    void Dot(const ImVec4& color)
    {
        const float r = ImGui::GetTextLineHeight() * 0.28f;
        const ImVec2 p = ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + r, p.y + ImGui::GetTextLineHeight() * 0.5f), r, Col(color));
        ImGui::Dummy(ImVec2(r * 2.0f, ImGui::GetTextLineHeight()));
    }
}
