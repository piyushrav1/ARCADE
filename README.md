# Arcade Hub (C++ / Win32)

A zero-dependency, software-rendered multi-game arcade platform compiled into a **single Windows executable** (`ArcadeHub.exe`).

## Included Games

- **Pong Arena:** 1P vs AI (Easy/Medium/Hard) or 2P local, configurable win scores, power-ups (`Big Paddle`, `Multi-Ball`, `Speed`), and 3 themes.
- **Neon Breaker:** 8 handcrafted levels with 1/2/3-hit bricks, steel blocks, chain-reaction TNT, combo multipliers, and power-ups (`Wide`, `Multi-Ball`, `Laser`, `+Life`).

## Structure

```text
├── src/
│   ├── core/          # Win32 window, input, software renderer, waveOut BGM/SFX mixer
│   ├── games/         # Namespaced game modules (pong.cpp, breaker.cpp)
│   └── hub.cpp        # Splash screen, game selector, and scene switcher
├── CMakeLists.txt     # CMake build configuration
└── build_release.ps1  # Builds stripped single-file dist/ArcadeHub.exe
```

## Build

```powershell
.\build_release.ps1
```
Output: `dist\ArcadeHub.exe` (standalone, no external assets or source files required).

