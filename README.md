# Ham's Cube Game

**Ham's Cube Game v0.01a3** is a small C++ game prototype built with [raylib](https://www.raylib.com/). This version gives the home screen a quieter 2D look and replaces Test 1 with Snake.

## Current progress

**Status: early playable prototype � v0.01a3**

### Done

- C++ desktop game shell with a CMake build, Raylib 6.2, and a visible version label.
- Warm, mostly 2D home screen with a gently animated title, custom Raylib-drawn Snake banner art, rounded test buttons, and �by hamdev� credit.
- Test 1 is a playable Snake game. Green food adds one segment; yellow food adds three and speeds Snake up for five seconds. The speed bar shows the remaining boost.
- Best Snake score and settings are stored in a shared per-user data folder, so they survive replacing the game executable with a later version. Existing beside-executable settings migrate on first launch.
- Settings for sound volume, window resolution, fullscreen, dark mode, target FPS (capped at the monitor refresh rate), FPS counter, and TTF/OTF font loading. The font row fits its filename and chooser; missing UI glyphs use the default font and show a warning.
- Test 2 (Cube Lab) and Test 3 (Cube Hop) remain small 3D prototypes.
- Windows workflow that builds the game for pushes and pull requests, then uploads a downloadable ZIP artifact.

### Not done yet

- The test modes are small prototypes; the project does not yet have a longer game mode with levels or a complete progression loop.
- Controller support and key remapping are not implemented.
- Snake currently has one board and no pause or difficulty selection.

### Likely next steps

1. Add a pause option and a few difficulty choices to Snake.
2. Decide whether Test 2 and Test 3 should grow into full 2D games or be replaced.
3. Add controller support and configurable controls.
4. Publish a versioned Windows release alongside the Actions build artifact.

## Build on Windows

Install [CMake](https://cmake.org/download/) and Visual Studio 2022 or newer with the **Desktop development with C++** workload. From a terminal in this folder:

```powershell
cmake -S . -B build -G "Visual Studio 18 2026"
cmake --build build --config Release
```

If your Visual Studio version uses a different CMake generator, list the available generators with `cmake -G` and replace the generator name above. CMake downloads raylib 6.2 the first time it configures.

Run `build\Release\hams-cube-game.exe`. Keep the `assets` folder beside the executable. On Windows, settings and the best Snake score live under `%APPDATA%\Hamdev\HamsCubeGame`; the game imports a legacy `settings.ini` from beside the executable once if no shared settings file exists.

## Controls

- **Test 1 � Snake:** Arrow keys or WASD steer; avoid the walls and your tail. Green food adds one tail segment. Yellow food adds three and gives a five-second speed boost. Press R or Enter after game over to restart.
- **Test 2 � Cube Lab:** Move the mouse to turn the cube and click to change its colour.
- **Test 3 � Cube Hop:** Space or click to hop; land on the platform to build your streak.
- **Escape:** Return to the home screen.

## Settings

Adjust sound volume, window size, fullscreen, dark mode, frame-rate limit, and the FPS counter. Unlimited and above-refresh presets are capped at the monitor�s refresh rate. On Windows, **Choose font** opens a file picker for `.ttf` and `.otf` fonts. You can also drag either font format onto the game window. The included Daydream OTF is used by default. If a chosen font lacks UI glyphs, those strings fall back to the system font and a notice appears in settings.

## License

The game source is MIT licensed; see [LICENSE](LICENSE). Raylib is distributed under its own zlib/libpng license; see [licenses/raylib.txt](licenses/raylib.txt). The included Daydream font is copyright DoubleGum (2020); its separate font note is in [assets/FONT-LICENSE.txt](assets/FONT-LICENSE.txt) and applies only to that font.

