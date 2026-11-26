#include "raylib.h"
#include "font_picker.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {
constexpr int START_WIDTH = 1360;
constexpr int START_HEIGHT = 820;
constexpr const char* VERSION = "v0.01a";
constexpr std::array<int, 4> FPS_OPTIONS{30, 60, 120, 144};
constexpr std::array<Vector2, 3> LANES{{{-2.0f, 0.0f}, {0.0f, 0.0f}, {2.0f, 0.0f}}};
constexpr std::array<Color, 5> CUBE_COLORS{{{74, 220, 255, 255}, {255, 101, 127, 255}, {177, 124, 255, 255}, {255, 199, 84, 255}, {116, 239, 163, 255}}};

struct Settings {
    int volume = 72;
    int resolution = 1;
    int fps = 1;
    bool fullscreen = false;
    bool dark = true;
    bool showFps = false;
    std::string fontPath;
};

struct Obstacle { int lane; float z; };

enum class Page { Home, Settings, Test };

Settings settings;
Font gameFont{};
bool ownsFont = false;
Sound clickSound{};
Sound bonkSound{};
Page page = Page::Home;
int activeTest = 1;
int lane = 1;
int colorIndex = 0;
float elapsed = 0;
float spawnTimer = 0;
float jumpY = 0;
float jumpVelocity = 0;
float hopY = 0;
float hopVelocity = 0;
int score = 0;
bool gameOver = false;
std::vector<Obstacle> obstacles;
std::mt19937 randomEngine{std::random_device{}()};

const std::array<int, 3> RESOLUTIONS_W{1280, 1600, 1920};
const std::array<int, 3> RESOLUTIONS_H{720, 900, 1080};

std::filesystem::path appPath(const std::string& relative) {
    const char* raw = GetApplicationDirectory();
    std::filesystem::path root = raw ? raw : "./";
    return root / relative;
}

Color bg() { return settings.dark ? Color{12, 15, 28, 255} : Color{237, 243, 251, 255}; }
Color panel() { return settings.dark ? Color{24, 29, 48, 238} : Color{255, 255, 255, 245}; }
Color ink() { return settings.dark ? Color{240, 244, 255, 255} : Color{28, 35, 54, 255}; }
Color muted() { return settings.dark ? Color{149, 160, 187, 255} : Color{96, 108, 131, 255}; }

void text(const std::string& value, float x, float y, float size, Color color = ink()) {
    DrawTextEx(gameFont, value.c_str(), {x, y}, size, 1.0f, color);
}

float textWidth(const std::string& value, float size) {
    return MeasureTextEx(gameFont, value.c_str(), size, 1.0f).x;
}

void centered(const std::string& value, float x, float y, float size, Color color = ink()) {
    text(value, x - textWidth(value, size) / 2.0f, y, size, color);
}

bool hovered(Rectangle rect) { return CheckCollisionPointRec(GetMousePosition(), rect); }

bool button(Rectangle rect, const std::string& label, bool accent = false) {
    const bool over = hovered(rect);
    Color face = accent ? Color{53, 82, 133, 255} : (settings.dark ? Color{35, 42, 65, 255} : Color{230, 236, 246, 255});
    if (over) face = accent ? Color{69, 111, 181, 255} : (settings.dark ? Color{47, 58, 88, 255} : Color{218, 228, 242, 255});
    DrawRectangleRounded(rect, 0.24f, 12, face);
    DrawRectangleRoundedLinesEx(rect, 0.24f, 12, 1.2f, over ? CUBE_COLORS[0] : (settings.dark ? Color{66, 76, 105, 255} : Color{201, 211, 228, 255}));
    centered(label, rect.x + rect.width / 2.0f, rect.y + (rect.height - 26) / 2.0f, 26, ink());
    if (over && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        if (IsSoundValid(clickSound)) PlaySound(clickSound);
        return true;
    }
    return false;
}

Wave tone(float frequency, float seconds, float volume) {
    Wave wave{};
    wave.frameCount = static_cast<unsigned int>(44100 * seconds);
    wave.sampleRate = 44100;
    wave.sampleSize = 16;
    wave.channels = 1;
    auto* samples = static_cast<std::int16_t*>(MemAlloc(wave.frameCount * sizeof(std::int16_t)));
    for (unsigned int i = 0; i < wave.frameCount; ++i) {
        const float t = static_cast<float>(i) / 44100.0f;
        const float fade = 1.0f - static_cast<float>(i) / static_cast<float>(wave.frameCount);
        samples[i] = static_cast<std::int16_t>(std::sin(2.0f * PI * frequency * t) * volume * fade * 32767.0f);
    }
    wave.data = samples;
    return wave;
}

Sound makeTone(float frequency, float seconds, float volume) {
    Wave wave = tone(frequency, seconds, volume);
    Sound sound = LoadSoundFromWave(wave);
    UnloadWave(wave);
    return sound;
}

void saveSettings() {
    std::ofstream file(appPath("settings.ini"));
    if (!file) return;
    file << "volume=" << settings.volume << '\n'
         << "resolution=" << settings.resolution << '\n'
         << "fps=" << settings.fps << '\n'
         << "fullscreen=" << settings.fullscreen << '\n'
         << "dark=" << settings.dark << '\n'
         << "showFps=" << settings.showFps << '\n'
         << "font=" << settings.fontPath << '\n';
}

void loadSettings() {
    std::ifstream file(appPath("settings.ini"));
    std::string line;
    while (std::getline(file, line)) {
        const auto split = line.find('=');
        if (split == std::string::npos) continue;
        const std::string key = line.substr(0, split);
        const std::string value = line.substr(split + 1);
        try {
            if (key == "volume") settings.volume = std::clamp(std::stoi(value), 0, 100);
            else if (key == "resolution") settings.resolution = std::clamp(std::stoi(value), 0, 2);
            else if (key == "fps") settings.fps = std::clamp(std::stoi(value), 0, static_cast<int>(FPS_OPTIONS.size()));
            else if (key == "fullscreen") settings.fullscreen = std::stoi(value) != 0;
            else if (key == "dark") settings.dark = std::stoi(value) != 0;
            else if (key == "showFps") settings.showFps = std::stoi(value) != 0;
            else if (key == "font") settings.fontPath = value;
        } catch (...) {}
    }
}

bool loadGameFont(const std::string& path) {
    if (path.empty() || !FileExists(path.c_str())) return false;
    Font next = LoadFontEx(path.c_str(), 64, nullptr, 0);
    if (next.texture.id == 0) return false;
    SetTextureFilter(next.texture, TEXTURE_FILTER_BILINEAR);
    if (ownsFont) UnloadFont(gameFont);
    gameFont = next;
    ownsFont = true;
    settings.fontPath = path;
    return true;
}

void setFullscreen(bool enabled) {
    if (settings.fullscreen == enabled) return;
    if (IsWindowFullscreen()) ToggleFullscreen();
    SetWindowSize(RESOLUTIONS_W[settings.resolution], RESOLUTIONS_H[settings.resolution]);
    if (enabled) ToggleFullscreen();
    settings.fullscreen = enabled;
}

void applyFps() { SetTargetFPS(settings.fps < static_cast<int>(FPS_OPTIONS.size()) ? FPS_OPTIONS[settings.fps] : 0); }

void startTest(int test) {
    page = Page::Test;
    activeTest = test;
    elapsed = spawnTimer = jumpY = jumpVelocity = hopY = hopVelocity = 0;
    score = 0;
    lane = 1;
    gameOver = false;
    obstacles.clear();
}

void drawBackground(float time) {
    const int height = GetScreenHeight();
    DrawRectangleGradientV(0, 0, GetScreenWidth(), height, settings.dark ? Color{17, 21, 39, 255} : Color{245, 248, 253, 255}, bg());
    for (int i = 0; i < 7; ++i) {
        const float x = std::fmod(i * 239.0f + time * 8.0f, static_cast<float>(GetScreenWidth() + 50));
        DrawCircleV({x, 100.0f + (i % 3) * 213.0f}, 2.0f + (i % 3), Color{96, 183, 255, 30});
    }
    for (int x = 0; x < GetScreenWidth(); x += 64) DrawLine(x, 0, x, height, settings.dark ? Color{130, 153, 199, 10} : Color{99, 123, 156, 12});
    for (int y = 0; y < height; y += 64) DrawLine(0, y, GetScreenWidth(), y, settings.dark ? Color{130, 153, 199, 10} : Color{99, 123, 156, 12});
}

void drawHome(float time) {
    const float width = static_cast<float>(GetScreenWidth());
    centered("ham's", width / 2.0f + std::sin(time * 13.0f) * 3.0f, 78 + std::sin(time * 9.0f) * 2.0f, 76, Color{255, 93, 115, 255});
    centered("cube game!", width / 2.0f, 146, 52, ink());
    centered("A tiny world with a big cube problem.", width / 2.0f, 211, 22, muted());
    DrawRectangleRounded({width / 2.0f - 164, 272, 328, 300}, 0.15f, 16, panel());
    centered("YOUR CUBE IS READY", width / 2.0f, 293, 19, muted());

    Camera3D camera{{4.8f, 4.2f, 7.2f}, {0, 0, 0}, {0, 1, 0}, 38.0f, CAMERA_PERSPECTIVE};
    BeginMode3D(camera);
    DrawPlane({0, -1.0f, 0}, {200, 200}, settings.dark ? Color{27, 33, 53, 255} : Color{218, 226, 239, 255});
    DrawGrid(20, 1.0f);
    const Vector3 cubePos{std::sin(time * 0.8f) * 0.22f, std::sin(time * 2.0f) * 0.15f, 0};
    DrawCubeV(cubePos, {1.65f, 1.65f, 1.65f}, CUBE_COLORS[0]);
    DrawCubeWiresV(cubePos, {1.65f, 1.65f, 1.65f}, Color{255, 255, 255, 255});
    EndMode3D();

    const float left = 76;
    const float right = width - 356;
    centered("PLAY TESTS", left + 140, 354, 20, muted());
    centered("TWEAK IT", right + 140, 354, 20, muted());
    if (button({left, 395, 280, 62}, "Test 1  /  Cube Dash", true)) startTest(1);
    if (button({left, 473, 280, 62}, "Test 2  /  Cube Lab")) startTest(2);
    if (button({left, 551, 280, 62}, "Test 3  /  Cube Hop")) startTest(3);
    if (button({right, 395, 280, 62}, "Settings", true)) page = Page::Settings;
    if (button({right, 473, 280, 62}, "Test 2  /  Cube Lab")) startTest(2);
    if (button({right, 551, 280, 62}, "Test 3  /  Cube Hop")) startTest(3);
    centered("HAM'S CUBE GAME", width / 2.0f, static_cast<float>(GetScreenHeight() - 42), 17, muted());
    DrawText(VERSION, 26, GetScreenHeight() - 37, 18, CUBE_COLORS[0]);
}

void drawSettings() {
    const float width = static_cast<float>(GetScreenWidth());
    const Rectangle box{width / 2.0f - 430, 44, 860, 720};
    DrawRectangleRounded(box, 0.08f, 14, panel());
    DrawRectangleRoundedLinesEx(box, 0.08f, 14, 1.5f, settings.dark ? Color{59, 71, 104, 255} : Color{199, 211, 231, 255});
    text("SETTINGS", box.x + 42, box.y + 24, 42);
    text("Make the cube feel like yours.", box.x + 42, box.y + 76, 20, muted());
    const float labelX = box.x + 44;
    const float controlX = box.x + 418;
    float y = box.y + 138;
    const float row = 76;

    text("SFX volume", labelX, y + 10, 25);
    DrawRectangleRounded({controlX, y + 15, 260, 14}, 0.5f, 8, settings.dark ? Color{49, 57, 78, 255} : Color{218, 226, 239, 255});
    const float volumeWidth = 260.0f * static_cast<float>(settings.volume) / 100.0f;
    DrawRectangleRounded({controlX, y + 15, volumeWidth, 14}, 0.5f, 8, CUBE_COLORS[0]);
    DrawCircleV({controlX + volumeWidth, y + 22}, 11, RAYWHITE);
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), {controlX - 12, y, 284, 44})) {
        settings.volume = std::clamp(static_cast<int>((GetMouseX() - controlX) * 100.0f / 260.0f), 0, 100);
        SetMasterVolume(settings.volume / 100.0f);
        saveSettings();
    }
    text(std::to_string(settings.volume) + "%", controlX + 282, y + 7, 21, muted());
    y += row;

    text("Window resolution", labelX, y + 10, 25);
    if (button({controlX, y, 62, 48}, "<")) {
        settings.resolution = (settings.resolution + 2) % 3;
        if (settings.fullscreen) setFullscreen(false), setFullscreen(true);
        else SetWindowSize(RESOLUTIONS_W[settings.resolution], RESOLUTIONS_H[settings.resolution]);
        saveSettings();
    }
    const std::string resolution = std::to_string(RESOLUTIONS_W[settings.resolution]) + " x " + std::to_string(RESOLUTIONS_H[settings.resolution]);
    centered(resolution, controlX + 184, y + 11, 22);
    if (button({controlX + 306, y, 62, 48}, ">")) {
        settings.resolution = (settings.resolution + 1) % 3;
        if (settings.fullscreen) setFullscreen(false), setFullscreen(true);
        else SetWindowSize(RESOLUTIONS_W[settings.resolution], RESOLUTIONS_H[settings.resolution]);
        saveSettings();
    }
    y += row;

    text("Fullscreen", labelX, y + 10, 25);
    if (button({controlX, y, 150, 48}, settings.fullscreen ? "ON" : "OFF", settings.fullscreen)) {
        setFullscreen(!settings.fullscreen);
        saveSettings();
    }
    y += row;

    text("Custom font", labelX, y + 10, 25);
    text(settings.fontPath.empty() ? "Daydream (default)" : GetFileName(settings.fontPath.c_str()), controlX, y + 12, 18, muted());
    if (button({controlX + 258, y, 150, 48}, "Choose font")) {
#ifdef _WIN32
        const std::string selected = ChooseFontFile();
        if (!selected.empty() && loadGameFont(selected)) saveSettings();
#endif
    }
    y += row;

    text("Dark mode", labelX, y + 10, 25);
    if (button({controlX, y, 150, 48}, settings.dark ? "ON" : "OFF", settings.dark)) {
        settings.dark = !settings.dark;
        saveSettings();
    }
    y += row;

    text("Target FPS", labelX, y + 10, 25);
    const std::string fpsLabel = settings.fps < static_cast<int>(FPS_OPTIONS.size()) ? std::to_string(FPS_OPTIONS[settings.fps]) + " FPS" : "Unlimited";
    if (button({controlX, y, 368, 48}, "<       " + fpsLabel + "       >")) {
        settings.fps = (settings.fps + 1) % (static_cast<int>(FPS_OPTIONS.size()) + 1);
        applyFps();
        saveSettings();
    }
    y += row;

    text("FPS counter", labelX, y + 10, 25);
    if (button({controlX, y, 150, 48}, settings.showFps ? "ON" : "OFF", settings.showFps)) {
        settings.showFps = !settings.showFps;
        saveSettings();
    }
    if (button({box.x + 42, box.y + box.height - 64, 170, 46}, "Back")) page = Page::Home;
    centered("TTF and OTF fonts are both supported.", width / 2.0f, box.y + box.height - 55, 18, muted());
}

void drawTestOne(float dt) {
    if (!gameOver) {
        elapsed += dt;
        const float speed = std::min(13.0f, 7.0f + elapsed * 0.16f);
        spawnTimer -= dt;
        if (spawnTimer <= 0) {
            std::uniform_int_distribution<int> chooseLane(0, 2);
            obstacles.push_back({chooseLane(randomEngine), 19.0f});
            spawnTimer = std::max(0.58f, 1.18f - elapsed * 0.009f);
        }
        for (auto& obstacle : obstacles) obstacle.z -= speed * dt;
        if (IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) lane = std::max(0, lane - 1);
        if (IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) lane = std::min(2, lane + 1);
        if (IsKeyPressed(KEY_SPACE) && jumpY <= 0.01f) jumpVelocity = 7.0f;
        jumpY += jumpVelocity * dt;
        jumpVelocity -= 18.0f * dt;
        if (jumpY < 0) jumpY = jumpVelocity = 0;
        for (const auto& obstacle : obstacles) {
            if (obstacle.lane == lane && std::abs(obstacle.z - 2.2f) < 0.85f && jumpY < 0.55f) gameOver = true;
        }
        const auto before = obstacles.size();
        std::erase_if(obstacles, [](const Obstacle& obstacle) { return obstacle.z < -1.0f; });
        score += static_cast<int>(before - obstacles.size());
        if (gameOver && IsSoundValid(bonkSound)) PlaySound(bonkSound);
    }

    Camera3D camera{{0, 8.0f, 12.0f}, {0, 0, 3.0f}, {0, 1, 0}, 48.0f, CAMERA_PERSPECTIVE};
    BeginMode3D(camera);
    DrawPlane({0, -0.6f, 5.0f}, {9.0f, 34.0f}, settings.dark ? Color{27, 34, 54, 255} : Color{207, 218, 235, 255});
    for (int x = -1; x <= 1; ++x) DrawLine3D({x * 2.0f, -0.58f, -10}, {x * 2.0f, -0.58f, 20}, Color{125, 151, 194, 130});
    for (int i = 0; i < 2; ++i) DrawCubeV({(i == 0 ? -3.2f : 3.2f), 0, 1}, {0.12f, 0.12f, 2.0f}, CUBE_COLORS[0]);
    for (const auto& obstacle : obstacles) DrawCubeV({LANES[obstacle.lane].x, 0, obstacle.z}, {0.85f, 0.85f, 0.85f}, CUBE_COLORS[1]);
    DrawCubeV({LANES[lane].x, jumpY + 0.2f, 2.2f}, {0.8f, 0.8f, 0.8f}, CUBE_COLORS[0]);
    DrawCubeWiresV({LANES[lane].x, jumpY + 0.2f, 2.2f}, {0.8f, 0.8f, 0.8f}, RAYWHITE);
    EndMode3D();
}

void drawTestTwo(float time) {
    const float mx = static_cast<float>(GetMouseX()) / static_cast<float>(std::max(1, GetScreenWidth()));
    const float my = static_cast<float>(GetMouseY()) / static_cast<float>(std::max(1, GetScreenHeight()));
    Camera3D camera{{std::sin((mx - 0.5f) * 3.2f) * 6.0f, 2.8f + (0.5f - my) * 3.0f, std::cos((mx - 0.5f) * 3.2f) * 6.0f}, {0, 0, 0}, {0, 1, 0}, 42.0f, CAMERA_PERSPECTIVE};
    BeginMode3D(camera);
    DrawPlane({0, -1.0f, 0}, {200, 200}, settings.dark ? Color{25, 32, 52, 255} : Color{218, 226, 239, 255});
    DrawGrid(20, 1.0f);
    const Vector3 cube{0, std::sin(time * 2.0f) * 0.12f, 0};
    DrawCubeV(cube, {2.0f, 2.0f, 2.0f}, CUBE_COLORS[colorIndex]);
    DrawCubeWiresV(cube, {2.0f, 2.0f, 2.0f}, RAYWHITE);
    EndMode3D();
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) colorIndex = (colorIndex + 1) % static_cast<int>(CUBE_COLORS.size());
}

void drawTestThree(float dt) {
    if (hopY == 0 && (IsKeyPressed(KEY_SPACE) || IsMouseButtonPressed(MOUSE_BUTTON_LEFT))) hopVelocity = 8.0f;
    hopY += hopVelocity * dt;
    hopVelocity -= 17.0f * dt;
    if (hopY < 0) {
        if (hopVelocity < -2.0f) { ++score; if (IsSoundValid(clickSound)) PlaySound(clickSound); }
        hopY = hopVelocity = 0;
    }
    Camera3D camera{{4.0f, 3.4f, 6.0f}, {0, 0, 0}, {0, 1, 0}, 40.0f, CAMERA_PERSPECTIVE};
    BeginMode3D(camera);
    DrawPlane({0, -0.75f, 0}, {200, 200}, settings.dark ? Color{25, 32, 52, 255} : Color{218, 226, 239, 255});
    DrawGrid(20, 1.0f);
    DrawCubeV({0, -0.45f, 0}, {4.0f, 0.5f, 4.0f}, Color{59, 74, 106, 255});
    DrawCubeWiresV({0, -0.45f, 0}, {4.0f, 0.5f, 4.0f}, CUBE_COLORS[0]);
    DrawCubeV({0, hopY + 0.2f, 0}, {0.8f, 0.8f, 0.8f}, CUBE_COLORS[2]);
    DrawCubeWiresV({0, hopY + 0.2f, 0}, {0.8f, 0.8f, 0.8f}, RAYWHITE);
    EndMode3D();
}

void drawTest(float dt) {
    if (IsKeyPressed(KEY_ESCAPE)) { page = Page::Home; return; }
    const std::string title = activeTest == 1 ? "CUBE DASH" : activeTest == 2 ? "CUBE LAB" : "CUBE HOP";
    if (activeTest == 1) drawTestOne(dt);
    else if (activeTest == 2) drawTestTwo(static_cast<float>(GetTime()));
    else drawTestThree(dt);

    DrawRectangleRounded({22, 20, 310, 108}, 0.15f, 12, panel());
    text(title, 42, 34, 28);
    text(activeTest == 1 ? "Blocks dodged: " + std::to_string(score) : "Streak: " + std::to_string(score), 42, 73, 20, muted());
    if (activeTest == 1) centered("Move  ← →     Jump  SPACE", GetScreenWidth() / 2.0f, 30, 20, muted());
    else if (activeTest == 2) centered("Move the mouse to orbit  •  Click to recolour", GetScreenWidth() / 2.0f, 30, 20, muted());
    else centered("Press SPACE or click to hop", GetScreenWidth() / 2.0f, 30, 20, muted());
    if (activeTest == 1 && gameOver) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Color{7, 10, 20, 175});
        centered("CUBE CRASH!", GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f - 74, 50, Color{255, 101, 127, 255});
        centered("Press R to run it back  •  ESC for home", GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f + 4, 23);
        if (IsKeyPressed(KEY_R)) startTest(1);
    }
}
} // namespace

int main() {
    loadSettings();
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(RESOLUTIONS_W[settings.resolution], RESOLUTIONS_H[settings.resolution], "Ham's Cube Game v0.01a");
    SetExitKey(KEY_NULL);
    if (settings.fullscreen) ToggleFullscreen();
    SetTargetFPS(60);
    if (settings.fps < static_cast<int>(FPS_OPTIONS.size())) SetTargetFPS(FPS_OPTIONS[settings.fps]);
    else SetTargetFPS(0);
    if (settings.fontPath.empty()) settings.fontPath = appPath("assets/Daydream DEMO.otf").string();
    gameFont = GetFontDefault();
    loadGameFont(settings.fontPath);
    InitAudioDevice();
    clickSound = makeTone(660.0f, 0.055f, 0.24f);
    bonkSound = makeTone(145.0f, 0.22f, 0.32f);
    SetMasterVolume(settings.volume / 100.0f);

    float time = 0;
    while (!WindowShouldClose()) {
        const float dt = std::min(GetFrameTime(), 0.05f);
        time += dt;
        if (IsFileDropped()) {
            FilePathList files = LoadDroppedFiles();
            if (files.count > 0) {
                const std::string path = files.paths[0];
                const std::string ext = GetFileExtension(path.c_str());
                if (TextIsEqual(TextToLower(ext.c_str()), ".ttf") || TextIsEqual(TextToLower(ext.c_str()), ".otf")) {
                    if (loadGameFont(path)) saveSettings();
                }
            }
            UnloadDroppedFiles(files);
        }
        BeginDrawing();
        drawBackground(time);
        if (page == Page::Home) drawHome(time);
        else if (page == Page::Settings) {
            if (IsKeyPressed(KEY_ESCAPE)) page = Page::Home;
            drawSettings();
        } else drawTest(dt);
        if (settings.showFps) DrawFPS(GetScreenWidth() - 112, 20);
        EndDrawing();
    }

    saveSettings();
    if (ownsFont) UnloadFont(gameFont);
    if (IsSoundValid(clickSound)) UnloadSound(clickSound);
    if (IsSoundValid(bonkSound)) UnloadSound(bonkSound);
    CloseAudioDevice();
    CloseWindow();
    return 0;
}

