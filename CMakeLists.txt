cmake_minimum_required(VERSION 3.12)
project(KAYRA)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. Find Required Packages ---
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

# MAVSDK and OpenCV are not used by the GUI yet; link them when available.
find_package(MAVSDK QUIET)
find_package(OpenCV QUIET)

# --- 2. GLAD (bundled in dependencies/, works both natively and in Docker) ---
add_library(glad STATIC dependencies/src/glad.c)
target_include_directories(glad PUBLIC dependencies/include)

# --- 3. ImGui ---
add_library(imgui STATIC
        dependencies/imgui/imgui.cpp
        dependencies/imgui/imgui_tables.cpp
        dependencies/imgui/imgui_draw.cpp
        dependencies/imgui/imgui_demo.cpp
        dependencies/imgui/imgui_widgets.cpp
        dependencies/imgui/imgui_impl_glfw.cpp
        dependencies/imgui/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC dependencies/imgui)
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# --- 4. Executable ---
add_executable(KAYRA
        main.cpp
        Windows/Main_Window.cpp
        Windows/Main_Window.h
        Windows/Widgets_Controls.cpp
        Windows/Widgets_Controls.h
        Windows/Attitude_Indicator.cpp
        Windows/Attitude_Indicator.h
)

# Absolute asset path so the program runs from any working directory
target_compile_definitions(KAYRA PRIVATE KAYRA_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")

target_link_libraries(KAYRA PRIVATE glad imgui glfw OpenGL::GL ${CMAKE_DL_LIBS})

if(MAVSDK_FOUND)
    target_link_libraries(KAYRA PRIVATE MAVSDK::mavsdk)
endif()
if(OpenCV_FOUND)
    target_link_libraries(KAYRA PRIVATE ${OpenCV_LIBS})
endif()

# --- 5. Compile Options ---
if(NOT MSVC)
    target_compile_options(KAYRA PRIVATE -Wall -Wextra)
else()
    target_compile_options(KAYRA PRIVATE -W2)
endif()
