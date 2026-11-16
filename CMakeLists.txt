cmake_minimum_required(VERSION 3.12)
project(KAYRA LANGUAGES C CXX)
if(APPLE)
    enable_language(OBJCXX)   # native save dialog (Windows/File_Dialog_mac.mm)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

# Set C++ standard to C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- 1. Find Required Packages ---
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(CURL REQUIRED)     # downloads map tiles
find_package(Threads REQUIRED)  # tile download workers, MAVSDK connect thread

# MAVSDK provides live telemetry; without it the GUI still builds and shows "built without MAVSDK".
# OpenCV is not used yet.
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
target_include_directories(imgui SYSTEM PUBLIC dependencies/imgui)  # SYSTEM: hide warnings from ImGui headers
target_link_libraries(imgui PUBLIC glfw OpenGL::GL)

# --- 3b. imgui-node-editor (https://github.com/thedmd/imgui-node-editor, MIT) ---
add_library(imgui_node_editor STATIC
        dependencies/imgui-node-editor/imgui_node_editor.cpp
        dependencies/imgui-node-editor/imgui_node_editor_api.cpp
        dependencies/imgui-node-editor/imgui_canvas.cpp
        dependencies/imgui-node-editor/crude_json.cpp
)
target_include_directories(imgui_node_editor SYSTEM PUBLIC dependencies/imgui-node-editor)
target_link_libraries(imgui_node_editor PUBLIC imgui)

# --- 4. Executable ---
add_executable(KAYRA
        main.cpp
        Windows/Main_Window.cpp
        Windows/Main_Window.h
        Windows/Widgets_Controls.cpp
        Windows/Widgets_Controls.h
        Windows/Attitude_Indicator.cpp
        Windows/Attitude_Indicator.h
        Windows/backend.cpp
        Windows/backend.h
        Windows/Mission_Plan.cpp
        Windows/Mission_Plan.h
        Windows/Settings.cpp
        Windows/Settings.h
        Map/MapWidget.cpp
        Map/MapWidget.h
        Map/TileManager.cpp
        Map/TileManager.h
        Windows/Mission_Export.cpp
        Windows/Mission_Export.h
        Windows/File_Dialog.h
)
if(APPLE)
    target_sources(KAYRA PRIVATE Windows/File_Dialog_mac.mm)
    target_link_libraries(KAYRA PRIVATE "-framework AppKit")
else()
    target_sources(KAYRA PRIVATE Windows/File_Dialog.cpp)
    if(WIN32)
        target_link_libraries(KAYRA PRIVATE comdlg32)
    endif()
endif()
target_include_directories(KAYRA PRIVATE Map)

# Absolute asset path so the program runs from any working directory
target_compile_definitions(KAYRA PRIVATE KAYRA_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")

target_link_libraries(KAYRA PRIVATE glad imgui imgui_node_editor glfw OpenGL::GL CURL::libcurl Threads::Threads ${CMAKE_DL_LIBS})

if(MAVSDK_FOUND)
    target_link_libraries(KAYRA PRIVATE MAVSDK::mavsdk)
    target_compile_definitions(KAYRA PRIVATE KAYRA_HAS_MAVSDK)

    # Fake vehicle for testing without PX4/ArduPilot: ./build/kayra-sim
    add_executable(kayra-sim tools/kayra_sim.cpp)
    target_link_libraries(kayra-sim PRIVATE MAVSDK::mavsdk Threads::Threads)
else()
    message(WARNING "MAVSDK not found: building without telemetry support")
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
