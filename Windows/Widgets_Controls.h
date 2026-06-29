#pragma once
#include "glad/glad.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <GLFW/glfw3.h>
#include "stb_image.h"

namespace Widgets_Controls {
	bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height);
	static inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs);
	static inline ImVec2 ImRotate(const ImVec2& v, float cos_a, float sin_a);
	void ImageRotated(ImTextureID tex_id, ImVec2 center, ImVec2 size, float angle);
	void embraceTheDarkness();
}

