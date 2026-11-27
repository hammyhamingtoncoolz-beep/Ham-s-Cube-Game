#include "raylib.h"
#include "font_picker.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

namespace {
constexpr int START_WIDTH = 1360;
constexpr int START_HEIGHT = 820;
constexpr const char* VERSION = "v0.01a3";
constexpr std::array<int, 4> FPS_OPTIONS{30, 60, 120, 144};
constexpr std::array<Vector2, 3> LANES{{{-2.0f, 0.0f}, {0.0f, 0.0f}, {2.0f, 0.0f}}};
constexpr std::array<Color, 5> CUBE_COLORS{{{72, 170, 111, 255}, {232, 112, 87, 255}, {217, 158, 76, 255}, {115, 151, 196, 255}, {93, 177, 165, 255}}};
constexpr Color GREEN_FOOD{83, 190, 104, 255};
constexpr Color YELLOW_FOOD{246, 190, 66, 255};
constexpr int SNAKE_COLS = 24;
constexpr int SNAKE_ROWS = 18;

struct Settings {
    int volume = 72;
    int resolution = 1;
    int fps = 1;
    bool fullscreen = false;
    bool dark = false;
    bool showFps = false;
    std::string fontPath;
};

struct Obstacle { int lane; float z; };
struct Cell { int x; int y; bool operator==(const Cell& other) const { return x == other.x && y == other.y; } };
enum class FoodKind { Green, Yellow };

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
std::vector<Cell> snake;
Cell food{12, 8};
FoodKind foodKind = FoodKind::Green;
int snakeDirection = 1;
int snakeGrowth = 0;
int bestSnakeScore = 0;
float snakeTick = 0;
float speedBoost = 0;
bool customFontWarning = false;
std::vector<Obstacle> obstacles;
std::mt19937 randomEngine{std::random_device{}()};

const std::array<int, 3> RESOLUTIONS_W{1280, 1600, 1920};
const std::array<int, 3> RESOLUTIONS_H{720, 900, 1080};

std::filesystem::path appPath(const std::string& relative) {
    const char* raw = GetApplicationDirectory();
    std::filesystem::path root = raw ? raw : "./";
    return root / relative;
}

std::filesystem::path userDataPath(const std::string& relative) {
#ifdef _WIN32
    const char* roaming = std::getenv("APPDATA");
    std::filesystem::path root = roaming ? roaming : GetApplicationDirectory();
    root /= "Hamdev/HamsCubeGame";
#else
    const char* home = std::getenv("HOME");
    std::filesystem::path root = home ? home : ".";
    root /= ".local/share/Hamdev/HamsCubeGame";
#endif
    std::error_code error;
    std::filesystem::create_directories(root, error);
    return root / relative;
}

void migrateLegacyFile(const char* name) {
    const auto target = userDataPath(name);
    const auto legacy = appPath(name);
    std::error_code error;
    if (!std::filesystem::exists(target, error) && std::filesystem::exists(legacy, error))
        std::filesystem::copy_file(legacy, target, std::filesystem::copy_options::skip_existing, error);
}

Color bg() { return settings.dark ? Color{31, 40, 38, 255} : Color{244, 241, 230, 255}; }
Color panel() { return settings.dark ? Color{46, 57, 51, 248} : Color{255, 253, 246, 250}; }
Color ink() { return settings.dark ? Color{240, 241, 222, 255} : Color{49, 58, 48, 255}; }
Color muted() { return settings.dark ? Color{181, 194, 176, 255} : Color{111, 119, 99, 255}; }
Color accent() { return settings.dark ? Color{117, 190, 128, 255} : Color{75, 150, 91, 255}; }

bool fontSupports(const std::string& value) {
    if (gameFont.texture.id == 0 || gameFont.glyphCount <= 0) return true;
    for (unsigned char c : value) {
        if (c < 32 || c > 126) return false;
        const int index = GetGlyphIndex(gameFont, static_cast<int>(c));
        if (index < 0 || index >= gameFont.glyphCount || gameFont.glyphs[index].value != c) return false;
    }
    return true;
}

Font fontFor(const std::string& value) { return fontSupports(value) ? gameFont : GetFontDefault(); }

void text(const std::string& value, float x, float y, float size, Color color = ink()) {
    DrawTextEx(fontFor(value), value.c_str(), {x, y}, size, 1.0f, color);
}

float textWidth(const std::string& value, float size) {
    return MeasureTextEx(fontFor(value), value.c_str(), size, 1.0f).x;
}

void centered(const std::string& value, float x, float y, float size, Color color = ink()) {
    text(value, x - textWidth(value, size) / 2.0f, y, size, color);
}

bool hovered(Rectangle rect) { return CheckCollisionPointRec(GetMousePosition(), rect); }

bool button(Rectangle rect, const std::string& label, bool highlighted = false) {
    const bool over = hovered(rect);
    Color face = highlighted ? Color{92, 157, 101, 255} : (settings.dark ? Color{67, 78, 68, 255} : Color{236, 232, 217, 255});
    if (over) face = highlighted ? Color{112, 180, 118, 255} : (settings.dark ? Color{79, 94, 78, 255} : Color{226, 221, 202, 255});
    DrawRectangleRounded(rect, 0.24f, 12, face);
    DrawRectangleRoundedLinesEx(rect, 0.24f, 12, 1.2f, over ? accent() : (settings.dark ? Color{91, 108, 91, 255} : Color{210, 205, 185, 255}));
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
    const auto path = userDataPath("settings.ini");
    const auto temp = path.string() + ".tmp";
    std::ofstream file(temp, std::ios::trunc);
    if (!file) return;
    file << "format=1\n"
         << "volume=" << settings.volume << '\n'
         << "resolution=" << settings.resolution << '\n'
         << "fps=" << settings.fps << '\n'
         << "fullscreen=" << settings.fullscreen << '\n'
         << "dark=" << settings.dark << '\n'
         << "showFps=" << settings.showFps << '\n'
         << "font=" << settings.fontPath << '\n';
    file.close();
    std::error_code error;
    std::filesystem::rename(temp, path, error);
    if (error) { std::filesystem::remove(path, error); error.clear(); std::filesystem::rename(temp, path, error); }
}

void saveProfile() {
    const auto path = userDataPath("profile.ini");
    const auto temp = path.string() + ".tmp";
    std::ofstream file(temp, std::ios::trunc);
    if (!file) return;
    file << "format=1\nbestSnake=" << bestSnakeScore << '\n';
    file.close();
    std::error_code error;
    std::filesystem::rename(temp, path, error);
    if (error) { std::filesystem::remove(path, error); error.clear(); std::filesystem::rename(temp, path, error); }
}

void loadProfile() {
    std::ifstream file(userDataPath("profile.ini"));
    std::string line;
    while (std::getline(file, line)) if (line.rfind("bestSnake=", 0) == 0) {
        try { bestSnakeScore = std::max(0, std::stoi(line.substr(10))); } catch (...) {}
    }
}

void loadSettings() {
    migrateLegacyFile("settings.ini");
    std::ifstream file(userDataPath("settings.ini"));
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
    customFontWarning = !fontSupports("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789.,:;!?()[]{}+-=/_% ");
    return true;
}

void setFullscreen(bool enabled) {
    if (settings.fullscreen == enabled) return;
    if (IsWindowFullscreen()) ToggleFullscreen();
    SetWindowSize(RESOLUTIONS_W[settings.resolution], RESOLUTIONS_H[settings.resolution]);
    if (enabled) ToggleFullscreen();
    settings.fullscreen = enabled;
}

void applyFps() {
    const int requested = settings.fps < static_cast<int>(FPS_OPTIONS.size()) ? FPS_OPTIONS[settings.fps] : 0;
    const int refresh = GetMonitorRefreshRate(GetCurrentMonitor());
    SetTargetFPS(refresh > 0 ? (requested == 0 ? refresh : std::min(requested, refresh)) : requested);
}

void resetSnake();

void startTest(int test) {
    page = Page::Test;
    activeTest = test;
    elapsed = spawnTimer = jumpY = jumpVelocity = hopY = hopVelocity = 0;
    score = 0;
    lane = 1;
    gameOver = false;
    obstacles.clear();
    if (test == 1) resetSnake();
}

void drawBackground(float time) {
    const int height = GetScreenHeight();
    DrawRectangleGradientV(0, 0, GetScreenWidth(), height, settings.dark ? Color{39, 49, 44, 255} : Color{250, 247, 238, 255}, bg());
    for (int i = 0; i < 7; ++i) {
        const float x = std::fmod(i * 239.0f + time * 8.0f, static_cast<float>(GetScreenWidth() + 50));
        DrawCircleV({x, 100.0f + (i % 3) * 213.0f}, 2.0f + (i % 3), Color{102, 149, 107, 24});
    }
    // The home backdrop stays flat and quiet; no perspective grids or floating cubes.
}

void drawSnakeArt(float cx, float cy, float scale, float time) {
    const Color green = settings.dark ? Color{104, 157, 101, 255} : Color{139, 177, 115, 255};
    DrawEllipse(static_cast<int>(cx), static_cast<int>(cy + 48 * scale), 122 * scale, 22 * scale,
                settings.dark ? Color{20, 29, 24, 90} : Color{80, 98, 66, 42});
    const Vector2 points[] = {{cx-85*scale,cy+18*scale},{cx-47*scale,cy+18*scale},{cx-8*scale,cy+17*scale},{cx+30*scale,cy+1*scale},{cx+53*scale,cy-30*scale},{cx+82*scale,cy-29*scale}};
    for (int i = 0; i < 5; ++i) {
        DrawCircleV(points[i], 18 * scale, green);
        DrawCircleLines(static_cast<int>(points[i].x), static_cast<int>(points[i].y), 18 * scale, Color{54, 119, 71, 255});
    }
    const float bob = std::sin(time * 3.0f) * 2.0f * scale;
    DrawCircleV({points[5].x, points[5].y + bob}, 22 * scale, GREEN_FOOD);
    DrawCircleV({points[5].x - 5 * scale, points[5].y - 6 * scale + bob}, 5 * scale, Color{238, 255, 225, 230});
    DrawCircleV({points[5].x + 5 * scale, points[5].y - 3 * scale + bob}, 2.7f * scale, Color{39, 58, 45, 255});
    DrawCircleV({points[5].x + 12 * scale, points[5].y - 3 * scale + bob}, 2.7f * scale, Color{39, 58, 45, 255});
    DrawCircleV({cx - 99 * scale, cy - 5 * scale}, 10 * scale, YELLOW_FOOD);
    DrawRectangleRounded({cx - 96 * scale, cy - 19 * scale, 5 * scale, 13 * scale}, 0.5f, 4, Color{78, 139, 79, 255});
}

void drawHome(float time) {
    const float width = static_cast<float>(GetScreenWidth());
    centered("ham's", width/2 + std::sin(time*13)*2.5f, 46 + std::sin(time*9)*1.5f, 70, Color{209,79,70,255});
    centered("SNAKE GAME!", width/2, 112, 45, ink());
    centered("by hamdev", width/2, 163, 17, muted());
    const float left = 54, right = width - 304;
    text("PLAY", left + 10, 260, 18, muted()); text("MORE", right + 10, 260, 18, muted());
    if (button({left,296,250,62}, "Test 1 - Snake", true)) startTest(1);
    if (button({left,374,250,62}, "Test 2")) startTest(2);
    if (button({left,452,250,62}, "Test 3")) startTest(3);
    if (button({right,296,250,62}, "Settings", true)) page = Page::Settings;
    if (button({right,374,250,62}, "Test 2")) startTest(2);
    if (button({right,452,250,62}, "Test 3")) startTest(3);
    const Rectangle card{width/2 - 191,220,382,348};
    DrawRectangleRounded(card,0.12f,14,panel());
    DrawRectangleRoundedLinesEx(card,0.12f,14,1.5f,settings.dark?Color{90,118,91,255}:Color{218,213,191,255});
    centered("A LITTLE SNAKE ADVENTURE",width/2,242,16,muted());
    drawSnakeArt(width/2,390,1.13f,time);
    centered("GREEN +1      YELLOW +3",width/2,477,18,ink());
    centered("Yellow food gives a short speed boost.",width/2,508,15,muted());
    centered("Best run: " + std::to_string(bestSnakeScore),width/2,538,16,accent());
    centered(VERSION,width-53,static_cast<float>(GetScreenHeight()-34),17,accent());
    centered("by hamdev",width/2,static_cast<float>(GetScreenHeight()-36),17,muted());
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
    const Rectangle fontBox{controlX, y + 4, 242, 44};
    DrawRectangleRounded(fontBox, 0.14f, 8, settings.dark ? Color{62, 74, 63, 255} : Color{239, 236, 222, 255});
    DrawRectangleRoundedLinesEx(fontBox, 0.14f, 8, 1.0f, settings.dark ? Color{93, 110, 91, 255} : Color{215, 208, 186, 255});
    const std::string fontName = settings.fontPath.empty() ? "Daydream (default)" : GetFileName(settings.fontPath.c_str());
    std::string clippedFontName = fontName;
    while (clippedFontName.size() > 3 && textWidth(clippedFontName, 15) > fontBox.width - 18) clippedFontName.pop_back();
    if (clippedFontName != fontName) clippedFontName = clippedFontName.substr(0, clippedFontName.size() - 3) + "...";
    text(clippedFontName, fontBox.x + 9, fontBox.y + 13, 15, muted());
    if (button({controlX + 258, y, 150, 48}, "Choose font")) {
#ifdef _WIN32
        const std::string selected = ChooseFontFile();
        if (!selected.empty() && loadGameFont(selected)) saveSettings();
#endif
    }
    y += row;
    if (customFontWarning) {
        DrawRectangleRounded({controlX, y - 28, 408, 26}, 0.12f, 8, settings.dark ? Color{104, 79, 42, 255} : Color{250, 231, 179, 255});
        text("Font misses UI glyphs; fallback text is used.", controlX + 8, y - 24, 12, settings.dark ? Color{255, 229, 167, 255} : Color{105, 75, 31, 255});
    }

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

void placeFood() {
    std::uniform_int_distribution<int> xs(0, SNAKE_COLS - 1), ys(0, SNAKE_ROWS - 1), kinds(0, 3);
    do { food = {xs(randomEngine), ys(randomEngine)}; }
    while (std::find(snake.begin(), snake.end(), food) != snake.end());
    foodKind = kinds(randomEngine) == 0 ? FoodKind::Yellow : FoodKind::Green;
}

void resetSnake() {
    snake = {{8, 9}, {9, 9}, {10, 9}};
    snakeDirection = 1;
    snakeGrowth = 0;
    snakeTick = speedBoost = elapsed = 0;
    score = 0;
    gameOver = false;
    placeFood();
}

void drawTestOne(float dt) {
    if (!gameOver) {
        speedBoost = std::max(0.0f, speedBoost - dt);
        if ((IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W)) && snakeDirection != 2) snakeDirection = 0;
        if ((IsKeyPressed(KEY_RIGHT) || IsKeyPressed(KEY_D)) && snakeDirection != 3) snakeDirection = 1;
        if ((IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) && snakeDirection != 0) snakeDirection = 2;
        if ((IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_A)) && snakeDirection != 1) snakeDirection = 3;
        snakeTick += dt;
        const float interval = speedBoost > 0 ? 0.105f : 0.145f;
        if (snakeTick >= interval) {
            snakeTick -= interval;
            Cell head = snake.front();
            if (snakeDirection == 0) --head.y;
            else if (snakeDirection == 1) ++head.x;
            else if (snakeDirection == 2) ++head.y;
            else --head.x;
            const bool eating = head == food;
            if (head.x < 0 || head.x >= SNAKE_COLS || head.y < 0 || head.y >= SNAKE_ROWS) gameOver = true;
            const std::size_t checkCount = snake.size() - ((snakeGrowth == 0 && !eating) ? 1 : 0);
            for (std::size_t i = 0; i < checkCount; ++i) if (snake[i] == head) gameOver = true;
            if (!gameOver) {
                snake.insert(snake.begin(), head);
                if (eating) {
                    if (foodKind == FoodKind::Green) { ++score; ++snakeGrowth; }
                    else { score += 3; snakeGrowth += 3; speedBoost = 5.0f; }
                    if (IsSoundValid(clickSound)) PlaySound(clickSound);
                    placeFood();
                }
                if (snakeGrowth > 0) --snakeGrowth;
                else snake.pop_back();
                if (score > bestSnakeScore) { bestSnakeScore = score; saveProfile(); }
            } else {
                bestSnakeScore = std::max(bestSnakeScore, score);
                saveProfile();
                if (IsSoundValid(bonkSound)) PlaySound(bonkSound);
            }
        }
    } else if (IsKeyPressed(KEY_R) || IsKeyPressed(KEY_ENTER)) resetSnake();

    const float cellSize = std::min((GetScreenWidth() - 100.0f) / SNAKE_COLS, (GetScreenHeight() - 200.0f) / SNAKE_ROWS);
    const float boardW = cellSize * SNAKE_COLS, boardH = cellSize * SNAKE_ROWS;
    const float x = (GetScreenWidth() - boardW) / 2.0f, y = (GetScreenHeight() - boardH) / 2.0f + 43;
    centered("HAM'S SNAKE", GetScreenWidth()/2.0f, 18, 25, accent());
    text("SCORE " + std::to_string(score), x, y - 37, 16, ink());
    text("BEST " + std::to_string(bestSnakeScore), x + 128, y - 37, 16, muted());
    text("SPEED", x + boardW - 222, y - 38, 14, muted());
    DrawRectangleRounded({x + boardW - 165, y - 35, 165, 14}, 0.35f, 8, settings.dark ? Color{77,91,75,255} : Color{220,217,201,255});
    if (speedBoost > 0) DrawRectangleRounded({x + boardW - 165, y - 35, 165 * speedBoost / 5.0f, 14}, 0.35f, 8, YELLOW_FOOD);
    DrawRectangleRounded({x-8,y-8,boardW+16,boardH+16},0.04f,8,panel());
    for (int gy=0; gy<SNAKE_ROWS; ++gy) for (int gx=0; gx<SNAKE_COLS; ++gx) {
        Color tile = ((gx+gy)%2==0) ? (settings.dark?Color{54,69,57,255}:Color{230,235,211,255}) : (settings.dark?Color{49,63,52,255}:Color{222,229,202,255});
        DrawRectangle(static_cast<int>(x+gx*cellSize),static_cast<int>(y+gy*cellSize),static_cast<int>(cellSize+1),static_cast<int>(cellSize+1),tile);
    }
    const float fx=x+(food.x+0.5f)*cellSize, fy=y+(food.y+0.5f)*cellSize;
    DrawCircleV({fx,fy},cellSize*0.34f,foodKind==FoodKind::Green?GREEN_FOOD:YELLOW_FOOD);
    if (foodKind==FoodKind::Yellow) DrawRectangleRounded({fx-2,fy-cellSize*0.49f,4,cellSize*0.22f},0.5f,4,Color{107,145,74,255});
    for (std::size_t i=snake.size(); i-->0;) {
        const Cell part=snake[i];
        DrawRectangleRounded({x+part.x*cellSize+2,y+part.y*cellSize+2,cellSize-4,cellSize-4},0.28f,6,i==0?Color{57,137,79,255}:Color{91,170,102,255});
    }
    centered("Move with arrows or WASD     ESC for home",GetScreenWidth()/2.0f,y+boardH+20,16,muted());
    if (gameOver) {
        DrawRectangle(0,0,GetScreenWidth(),GetScreenHeight(),Color{27,35,28,150});
        DrawRectangleRounded({GetScreenWidth()/2.0f-205,GetScreenHeight()/2.0f-82,410,164},0.12f,12,panel());
        centered("SNAKE GAME OVER",GetScreenWidth()/2.0f,GetScreenHeight()/2.0f-51,27,Color{209,79,70,255});
        centered("Final score: "+std::to_string(score),GetScreenWidth()/2.0f,GetScreenHeight()/2.0f-10,19);
        centered("R or ENTER to play again",GetScreenWidth()/2.0f,GetScreenHeight()/2.0f+28,16,muted());
    }
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
    if (activeTest == 1) drawTestOne(dt);
    else {
        if (activeTest == 2) drawTestTwo(static_cast<float>(GetTime()));
        else drawTestThree(dt);
        const std::string title = activeTest == 2 ? "TEST 2" : "TEST 3";
        DrawRectangleRounded({22,20,310,108},0.15f,12,panel());
        text(title,42,34,28);
        text("Streak: " + std::to_string(score),42,73,20,muted());
        if (activeTest == 2) centered("Move the mouse to orbit  -  Click to recolour",GetScreenWidth()/2.0f,30,19,muted());
        else centered("Press SPACE or click to hop",GetScreenWidth()/2.0f,30,19,muted());
    }
}
} // namespace

int main() {
    loadSettings();
    loadProfile();
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_MSAA_4X_HINT | FLAG_VSYNC_HINT);
    InitWindow(RESOLUTIONS_W[settings.resolution], RESOLUTIONS_H[settings.resolution], "Ham's Cube Game v0.01a3");
    SetExitKey(KEY_NULL);
    if (settings.fullscreen) ToggleFullscreen();
    applyFps();
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
