cmake_minimum_required(VERSION 3.15)
project(ArcadeLauncher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(launcher WIN32
    src/platform_win32.cpp
    src/renderer.cpp
    src/catalog.cpp
    src/launcher.cpp
)

target_include_directories(launcher PRIVATE src)

target_link_libraries(launcher PRIVATE
    user32
    gdi32
    winmm
)

