cmake_minimum_required(VERSION 3.12)
project(KAYRA)

set(CMAKE_BUILD_TYPE Debug)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)

# Find GLFW package
find_package(glfw3 REQUIRED)

# Add the executable
add_executable(KAYRA
        main.cpp
        Windows/Main_Window.cpp
        Windows/Main_Window.h
        Windows/Widgets_Controls.cpp
        Windows/Widgets_Controls.h
        Windows/backend.h
        Windows/backend.cpp)

# Add the Glad library
add_library(glad STATIC dependencies/src/glad.c)
target_include_directories(glad PUBLIC dependencies/include)

find_package(MAVSDK REQUIRED)

# Add the Imgui library
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

# Link GLFW and Glad libraries
target_link_libraries(KAYRA PRIVATE MAVSDK::mavsdk glfw glad dl imgui)

if(NOT MSVC)
    add_compile_options(KAYRA PRIVATE -Wall -Wextra)
else()
    add_compile_options(KAYRA PRIVATE -W2)
endif()
