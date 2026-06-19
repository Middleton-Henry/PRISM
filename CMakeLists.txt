cmake_minimum_required(VERSION 3.16)
project(imgui_app)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find GLFW
find_package(glfw3 REQUIRED)

# Find OpenCV
find_package(OpenCV REQUIRED)

# macOS frameworks
find_library(COCOA Cocoa REQUIRED)
find_library(IOKIT IOKit REQUIRED)
find_library(COREVIDEO CoreVideo REQUIRED)
find_library(OPENGL OpenGL REQUIRED)

include_directories(
    imgui
    imgui/backends
)

add_executable(imgui_app
    main.cpp
    ImGuiFileDialog/ImGuiFileDialog.cpp
    imgui/imgui.cpp
    imgui/imgui_draw.cpp
    imgui/imgui_tables.cpp
    imgui/imgui_widgets.cpp
    imgui/imgui_demo.cpp
    imgui/backends/imgui_impl_glfw.cpp
    imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui_app PRIVATE ${OpenCV_INCLUDE_DIRS})

target_link_libraries(imgui_app
    glfw
    ${OpenCV_LIBS}
    ${COCOA}
    ${IOKIT}
    ${COREVIDEO}
    ${OPENGL}
)