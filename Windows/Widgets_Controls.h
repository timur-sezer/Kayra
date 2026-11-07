#pragma once
#include "glad/glad.h"
#include "imgui.h"
#include <GLFW/glfw3.h>

namespace Widgets_Controls {
	bool LoadTextureFromFile(const char* filename, GLuint* out_texture, int* out_width, int* out_height);
	void ImageRotated(ImTextureID tex_id, ImVec2 center, ImVec2 size, float angle);
	void embraceTheDarkness();
}
