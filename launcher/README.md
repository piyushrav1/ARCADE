# Arcade Hub (C++ / Win32)

A zero-dependency, software-rendered **Arcade Game Launcher** built in C++ using the native Win32 API (`GDI`, `User32`, `WinMM`).

## Features

- **Boot Splash Screen:** Retro power-on beam, BIOS check, and `ARCADE HUB` logo reveal on startup (skippable with `Space`, `Enter`, or click).
- **Chiptune Menu Music & SFX:** Zero-dependency `waveOut` 22.05 kHz PCM synthesizer playing looping menu music alongside non-blocking UI SFX.
- **Mouse & Keyboard UI:** Hover/click game cards or header toggles (`THEME`, `MUSIC`), or navigate with the keyboard.
- **Config-Driven Catalog (`games.cfg`):** Discovers registered games and checks executable availability.
- **Seamless Launch & Return:** Launches the selected game in its own working directory, pauses menu music and hides the launcher while playing, and restores it on exit.

## Controls

| Action | Keys / Mouse |
| :--- | :--- |
| **Navigate Games** | `W` / `S`, `Up` / `Down`, or Mouse Hover |
| **Launch Game** | `Enter`, `Space`, or Click Card |
| **Toggle Music** | `B` or Click `MUSIC` |
| **Cycle Theme** | `T`, `Left` / `Right`, or Click Theme Pill |
| **Mute All Audio** | `M` |
| **Quit** | `Esc` |

## Adding a New Game

1. Place your new game in its own folder under `ARCADE/` (e.g. `ARCADE/snake/snake.exe`).
2. Add a new section to `launcher/games.cfg`:
   ```ini
   [snake]
   title=NEON SNAKE
   subtitle=RETRO GRID SURVIVAL
   players=1P
   exe=../snake/snake.exe
   workdir=../snake
   accent=0x4ADE80
   icon=GAME
   ```
3. Re-open `launcher.exe` — no recompilation needed.

## Build Instructions

### MinGW (`g++`)
```powershell
g++ -O2 -Wall -Werror -mwindows src/platform_win32.cpp src/renderer.cpp src/catalog.cpp src/launcher.cpp -Isrc -o launcher.exe -lgdi32 -luser32 -lwinmm
```

### CMake (MSVC / MinGW / Clang)
```powershell
cmake -B build
cmake --build build --config Release
```
