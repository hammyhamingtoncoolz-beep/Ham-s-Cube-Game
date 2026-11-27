# Ham's Cube Game

**Ham's Cube Game v0.01a2** is a small C++ cube playground with three playable test modes, a settings screen, and a wobbly title. It uses [raylib](https://www.raylib.com/) for graphics, input, and sound.

## Current progress

**Status: early playable prototype — v0.01a2**

### Done

- C++ desktop game shell with a CMake build, Raylib 6.2, and a visible version label.
- Home screen with the animated title, 2D background, rounded test buttons, and a “by hamdev” credit.
- Three playable tests: Test 1 (Cube Dash), Test 2 (Cube Lab), and Test 3 (Cube Hop).
- Settings for sound volume, window resolution, fullscreen, dark mode, target FPS (capped at the monitor refresh rate), FPS counter, and TTF/OTF font loading.
- Windows workflow that builds the game for pushes and pull requests, then uploads a downloadable ZIP artifact.

### Not done yet

- The test modes are small prototypes; the project does not yet have a longer game mode with levels or a complete progression loop.
- High scores and gameplay progress are not saved. Only settings persist.
- Controller support and key remapping are not implemented.

### Likely next steps

1. Choose the main game mode and expand it with more levels, obstacles, and a polished restart/pause loop.
2. Save best scores and gameplay progress.
3. Add controller support and configurable controls.
4. Publish a versioned Windows release alongside the Actions build artifact.

## Build on Windows

Install [CMake](https://cmake.org/download/) and Visual Studio 2022 or newer with the **Desktop development with C++** workload. From a terminal in this folder:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026"
cmake --build build --config Release
```

If your Visual Studio version uses a different CMake generator, list the available generators with `cmake -G` and replace the generator name above. CMake downloads raylib 6.2 the first time it configures.

Run `build\Release\hams-cube-game.exe`. The `assets` folder sits beside the executable and must stay with it. Settings are saved to `settings.ini` beside the executable.

## Controls

- **Test 1 — Cube Dash:** Left/right arrows switch lanes; Space jumps; dodge the blocks.
- **Test 2 — Cube Lab:** Move the mouse to turn the cube and click to change its colour.
- **Test 3 — Cube Hop:** Space or click to hop; land on the platform to build your streak.
- **Escape:** Return to the home screen.

## Settings

Adjust sound volume, window size, fullscreen, dark mode, frame-rate limit, and the FPS counter. Unlimited and above-refresh presets are capped at the monitor’s refresh rate. On Windows, **Choose font** opens a file picker for `.ttf` and `.otf` fonts. You can also drag either font format onto the game window. The included Daydream OTF is used by default.

## License

The game source is MIT licensed; see [LICENSE](LICENSE). Raylib is distributed under its own zlib/libpng license; see [licenses/raylib.txt](licenses/raylib.txt). The included Daydream font is copyright DoubleGum (2020); its separate font note is in [assets/FONT-LICENSE.txt](assets/FONT-LICENSE.txt) and applies only to that font.

