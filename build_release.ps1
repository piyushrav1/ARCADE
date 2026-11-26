$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$dist = Join-Path $root "dist"

if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Path $dist -Force | Out-Null

Write-Host "Building single-executable dist\ArcadeHub.exe..."
Push-Location $root
g++ -O2 -s -Wall -Werror -mwindows src/core/platform_win32.cpp src/core/renderer.cpp src/core/keybinds.cpp src/core/audio_ui.cpp src/core/graphics_ui.cpp src/core/save.cpp src/games/pong.cpp src/games/breaker.cpp src/hub.cpp -Isrc/core -Isrc/games -o dist/ArcadeHub.exe -lgdi32 -luser32 -lwinmm
$ec = $LASTEXITCODE
Pop-Location
if ($ec -ne 0) { exit $ec }

Get-Item (Join-Path $dist "ArcadeHub.exe") | Select-Object FullName, Length
