#include "Widgets_Controls.h"
#include "glad/glad.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

namespace Widgets_Controls {
    bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height)
    {
        // Load from file
        int image_width = 0;
        int image_height = 0;
        unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
        if (image_data == NULL)
            return false;

        // Create a OpenGL texture identifier
        GLuint image_texture;
        glGenTextures(1, &image_texture);
        glBindTexture(GL_TEXTURE_2D, image_texture);

        // Setup filtering parameters for display
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // This is required on WebGL for non power-of-two textures
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same

        // Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, image_width, image_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image_data);
        stbi_image_free(image_data);

        *out_texture = image_texture;
        *out_width = image_width;
        *out_height = image_height;

        return true;
    }
    static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs)
    {
        return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y);
    }
    static inline ImVec2 ImRotate(const ImVec2& v, float cos_a, float sin_a)
    {
        return ImVec2(v.x * cos_a - v.y * sin_a, v.x * sin_a + v.y * cos_a);
    }
    void ImageRotated(ImTextureID tex_id, ImVec2 center, ImVec2 size, float angle)
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        float cos_a = cosf(angle);
        float sin_a = sinf(angle);
        ImVec2 pos[4] =
        {
            center + ImRotate(ImVec2(-size.x * 0.5f, -size.y * 0.5f), cos_a, sin_a),
            center + ImRotate(ImVec2(+size.x * 0.5f, -size.y * 0.5f), cos_a, sin_a),
            center + ImRotate(ImVec2(+size.x * 0.5f, +size.y * 0.5f), cos_a, sin_a),
            center + ImRotate(ImVec2(-size.x * 0.5f, +size.y * 0.5f), cos_a, sin_a)
        };
        ImVec2 uvs[4] =
        {
            ImVec2(0.0f, 0.0f),
            ImVec2(1.0f, 0.0f),
            ImVec2(1.0f, 1.0f),
            ImVec2(0.0f, 1.0f)
        };

        draw_list->AddImageQuad(tex_id, pos[0], pos[1], pos[2], pos[3], uvs[0], uvs[1], uvs[2], uvs[3], IM_COL32_WHITE);
    }
    void embraceTheDarkness()
    {
        ImVec4* colors = ImGui::GetStyle().Colors;

        // Text
        colors[ImGuiCol_Text] = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

        // Backgrounds
        colors[ImGuiCol_WindowBg] = ImColor(20, 20, 20, 255).Value;
        colors[ImGuiCol_ChildBg] = ImColor(30, 30, 30, 255).Value;
        colors[ImGuiCol_PopupBg] = ImVec4(0.13f, 0.13f, 0.13f, 0.94f);

        // Borders
        colors[ImGuiCol_Border] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
        colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.24f);

        // Frames
        colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.20f, 0.54f);
        colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.25f, 0.74f);
        colors[ImGuiCol_FrameBgActive] = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);

        // Titles
        colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);
        colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.10f, 0.10f, 0.10f, 1.00f);

        // Menus
        colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);

        // Scrollbars
        colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.10f, 0.54f);
        colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.40f, 0.40f, 0.40f, 0.54f);
        colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.50f, 0.50f, 0.50f, 0.74f);
        colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.70f, 0.70f, 0.70f, 0.74f);

        // Controls
        colors[ImGuiCol_CheckMark] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
        colors[ImGuiCol_SliderGrab] = ImVec4(0.30f, 0.30f, 0.30f, 0.54f);
        colors[ImGuiCol_SliderGrabActive] = ImVec4(0.56f, 0.56f, 0.56f, 0.74f);

        // Buttons
        colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.20f, 0.54f);
        colors[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.30f, 0.30f, 0.74f);
        colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.35f, 1.00f);

        // Headers
        colors[ImGuiCol_Header] = ImVec4(0.30f, 0.30f, 0.30f, 0.52f);
        colors[ImGuiCol_HeaderHovered] = ImVec4(0.38f, 0.38f, 0.38f, 0.36f);
        colors[ImGuiCol_HeaderActive] = ImVec4(0.47f, 0.47f, 0.47f, 0.33f);

        // Separators
        colors[ImGuiCol_Separator] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
        colors[ImGuiCol_SeparatorHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.74f);
        colors[ImGuiCol_SeparatorActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

        // Resize grips
        colors[ImGuiCol_ResizeGrip] = ImVec4(0.28f, 0.28f, 0.28f, 0.29f);
        colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.44f, 0.44f, 0.44f, 0.74f);
        colors[ImGuiCol_ResizeGripActive] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

        // Tabs
        colors[ImGuiCol_Tab] = ImVec4(0.18f, 0.18f, 0.18f, 0.86f);
        colors[ImGuiCol_TabHovered] = ImVec4(0.28f, 0.28f, 0.28f, 0.80f);
        colors[ImGuiCol_TabActive] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_TabUnfocused] = ImVec4(0.15f, 0.15f, 0.15f, 0.52f);
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);

        // Docking
        colors[ImGuiCol_DockingPreview] = ImVec4(0.33f, 0.67f, 0.86f, 0.40f);
        colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);

        // Plots
        colors[ImGuiCol_PlotLines] = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered] = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram] = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered] = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);

        // Tables
        colors[ImGuiCol_TableHeaderBg] = ImVec4(0.25f, 0.25f, 0.25f, 0.52f);
        colors[ImGuiCol_TableBorderStrong] = ImVec4(0.31f, 0.31f, 0.31f, 0.50f);
        colors[ImGuiCol_TableBorderLight] = ImVec4(0.23f, 0.23f, 0.23f, 0.29f);
        colors[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt] = ImVec4(1.00f, 1.00f, 1.00f, 0.05f);

        // Misc
        colors[ImGuiCol_TextSelectedBg] = ImVec4(0.33f, 0.67f, 0.86f, 0.35f);
        colors[ImGuiCol_DragDropTarget] = ImVec4(0.33f, 0.67f, 0.86f, 0.90f);
        colors[ImGuiCol_NavHighlight] = ImVec4(0.33f, 0.67f, 0.86f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight] = ImVec4(0.33f, 0.67f, 0.86f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);


        ImGuiStyle& style = ImGui::GetStyle();
        
        
        style.WindowPadding = ImVec2(5.00f, 5.00f);
        style.FramePadding = ImVec2(8.00f, 5.00f);
        style.CellPadding = ImVec2(4.00f, 8.00f);
        style.ItemSpacing = ImVec2(5.00f, 5.00f);
        style.ItemInnerSpacing = ImVec2(6.00f, 6.00f);
        style.TouchExtraPadding = ImVec2(0.00f, 0.00f);
        style.IndentSpacing = 10;
        style.ScrollbarSize = 8;
        style.GrabMinSize = 4;
        style.WindowBorderSize = 0;
        style.ChildBorderSize = 0;
        style.PopupBorderSize = 0;
        style.FrameBorderSize = 0;
        style.TabBorderSize = 0;
        style.WindowRounding = 10;
        style.ChildRounding = 10;
        style.FrameRounding = 10;
        style.PopupRounding = 10;
		style.WindowTitleAlign = ImVec2(0.5f, 0.5f);
		style.SeparatorTextAlign = ImVec2(0.5f, 0.5f);
        style.ScrollbarRounding = 10;
        style.GrabRounding = 3;
        style.LogSliderDeadzone = 4;
        style.TabRounding = 4;
        
    }
    

}


