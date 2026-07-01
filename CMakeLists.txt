cmake_minimum_required(VERSION 3.12)
project(KAYRA)

set(CMAKE_BUILD_TYPE Debug)

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)

# --- 1. Find Required Packages ---
find_package(glfw3 REQUIRED)
find_package(MAVSDK REQUIRED)
find_package(OpenCV REQUIRED) # Added OpenCV
find_package(OpenGL REQUIRED) # Ensure OpenGL is found for linking

# --- 2. Add the Executable ---
add_executable(KAYRA
        main.cpp
        Windows/Main_Window.cpp
        Windows/Main_Window.h
        Windows/Widgets_Controls.cpp
        Windows/Widgets_Controls.h
)

# --- 3. Add the GLAD Library (Using Docker's system paths) ---
add_library(glad STATIC /usr/local/src/glad.c)
target_include_directories(glad PUBLIC /usr/local/include)

# --- 4. Add the ImGui Library ---
# (Assuming 'dependencies/imgui' is mounted into the container via volume)
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

# --- 5. Link Everything Together ---
target_link_libraries(KAYRA 
    PRIVATE 
    MAVSDK::mavsdk 
    glfw 
    glad 
    dl 
    imgui
    ${OpenCV_LIBS} # Link OpenCV libraries
    OpenGL::GL     # Link System OpenGL
)

# --- 6. Compile Options ---
if(NOT MSVC)
    target_compile_options(KAYRA PRIVATE -Wall -Wextra)
else()
    target_compile_options(KAYRA PRIVATE -W2)
endif()
