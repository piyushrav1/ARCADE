cmake_minimum_required(VERSION 3.15)
project(ArcadeHub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(ArcadeHub WIN32
    src/core/platform_win32.cpp
    src/core/renderer.cpp
    src/core/keybinds.cpp
    src/core/audio_ui.cpp
    src/games/pong.cpp
    src/games/breaker.cpp
    src/hub.cpp
)

target_include_directories(ArcadeHub PRIVATE src/core src/games)

target_link_libraries(ArcadeHub PRIVATE
    user32
    gdi32
    winmm
)
