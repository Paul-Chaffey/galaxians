#include "app/Controls.h"
#include "app/FrameStats.h"
#include "app/ScreenLayout.h"
#include "audio/Audio.h"
#include "game/Game.h"
#include "gfx/Renderer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cmath>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <string>
#include <vector>

namespace {

#ifdef SDL_PLATFORM_ANDROID
constexpr bool kAndroid = true;
#else
constexpr bool kAndroid = false;
#endif

// Open at 3x the arcade's native resolution.
constexpr int kWindowScale = 3;
constexpr int kWindowWidth = int(gfx::Renderer::kVirtualSize.width) * kWindowScale;
constexpr int kWindowHeight = int(gfx::Renderer::kVirtualSize.height) * kWindowScale;

// Game logic runs at the arcade's 60 Hz regardless of display refresh rate.
constexpr double kTickSeconds = 1.0 / 60.0;
// Avoid a "spiral of death" after a long stall (debugger, window drag).
constexpr double kMaxFrameSeconds = 0.25;
// Frame times this close to a whole number of display refreshes are treated
// as exact, so timer jitter can't cause a double tick followed by none.
constexpr double kVsyncSnapSeconds = 0.00025;

// Desktop builds keep their assets next to the executable. On Android they are
// packed into the APK, and SDL opens relative paths from there.
std::string atlasPath()
{
    if (kAndroid)
        return "atlas.png";
    const char* basePath = SDL_GetBasePath();
    return std::string(basePath ? basePath : "") + "assets/atlas.png";
}

// The high score lives in the per-user preferences folder SDL picks for the platform.
std::string highScorePath()
{
    char* pref = SDL_GetPrefPath("galaxians-remake", "galaxians");
    if (!pref)
        return {};
    std::string path = std::string(pref) + "highscore.txt";
    SDL_free(pref);
    return path;
}

int loadHighScore(const std::string& path, int fallback)
{
    std::ifstream file(path);
    int score = 0;
    return (file >> score) && score > 0 ? score : fallback;
}

void saveHighScore(const std::string& path, int score)
{
    std::ofstream file(path);
    if (!(file << score << '\n'))
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not save the high score to %s", path.c_str());
}

// Seconds per refresh of the display the window is on, or 0 if unknown.
double refreshInterval(SDL_Window* window)
{
    const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(window));
    return mode && mode->refresh_rate > 0 ? 1.0 / double(mode->refresh_rate) : 0.0;
}

double snapToVsync(double frameSeconds, double interval)
{
    if (interval <= 0)
        return frameSeconds;
    for (int refreshes = 1; refreshes <= 4; ++refreshes) {
        const double exact = interval * refreshes;
        if (std::abs(frameSeconds - exact) < kVsyncSnapSeconds)
            return exact;
    }
    return frameSeconds;
}

// What the app-lifecycle event watch needs. SDL never queues the background and
// foreground events: they reach only event watches, as they happen. Android can
// stop the process at any time once the app is in the background, and it takes
// the window's surface away, so both are dealt with there and then.
struct Lifecycle {
    gfx::Renderer& renderer;
    const game::Game& game;
    const std::string& scorePath;
    int& savedHighScore;
    bool background = false;
    bool resumed = false; // back from the background since the main loop last looked
};

bool SDLCALL onAppEvent(void* userdata, SDL_Event* event)
{
    auto& app = *static_cast<Lifecycle*>(userdata);
    if (event->type == SDL_EVENT_WILL_ENTER_BACKGROUND) {
        app.background = true;
        app.renderer.releaseSurface();
    } else if (event->type == SDL_EVENT_DID_ENTER_FOREGROUND) {
        app.background = false;
        app.resumed = true;
    }
    if ((event->type == SDL_EVENT_WILL_ENTER_BACKGROUND || event->type == SDL_EVENT_TERMINATING) &&
        app.game.highScore() != app.savedHighScore && !app.scorePath.empty()) {
        app.savedHighScore = app.game.highScore();
        saveHighScore(app.scorePath, app.savedHighScore);
    }
    return true;
}

// Watches for app-lifecycle events for exactly as long as it exists.
class LifecycleWatch {
public:
    explicit LifecycleWatch(Lifecycle& lifecycle)
        : lifecycle_(lifecycle)
    {
        SDL_AddEventWatch(onAppEvent, &lifecycle_);
    }
    ~LifecycleWatch() { SDL_RemoveEventWatch(onAppEvent, &lifecycle_); }

    LifecycleWatch(const LifecycleWatch&) = delete;
    LifecycleWatch& operator=(const LifecycleWatch&) = delete;

private:
    Lifecycle& lifecycle_;
};

// Lays out the playfield and the on-screen buttons for the window's current
// size, keeping clear of notches and system bars.
app::ScreenLayout layoutWindow(SDL_Window* window)
{
    int width = 0, height = 0;
    SDL_GetWindowSizeInPixels(window, &width, &height);
    SDL_Rect safe{0, 0, 0, 0};
    const float density = SDL_GetWindowPixelDensity(window);
    app::Rect safePixels{0, 0, float(width), float(height)};
    if (SDL_GetWindowSafeArea(window, &safe) && safe.w > 0 && safe.h > 0)
        safePixels = {float(safe.x) * density, float(safe.y) * density, float(safe.w) * density,
                      float(safe.h) * density};
    return app::layoutScreen(float(width), float(height), safePixels);
}

void toggleFullscreen(SDL_Window* window)
{
    const bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    SDL_SetWindowFullscreen(window, !fullscreen); // borderless desktop fullscreen
}

int run()
{
    // SDL hides non-application warnings by default; we want validation warnings.
    SDL_SetLogPriority(SDL_LOG_CATEGORY_GPU, SDL_LOG_PRIORITY_WARN);
    // Phones: any way up except upside-down portrait, which most don't allow anyway.
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight Portrait");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init: %s", SDL_GetError());
        return 1;
    }

    SDL_WindowFlags flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (kAndroid)
        flags |= SDL_WINDOW_FULLSCREEN;
    SDL_Window* window = SDL_CreateWindow("Galaxians", kWindowWidth, kWindowHeight, flags);
    if (!window) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_CreateWindow: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int status = 0;
    try {
        gfx::Renderer renderer(window, atlasPath());
        const std::string scorePath = highScorePath();
        int savedHighScore = loadHighScore(scorePath, 5000);
        game::Game game(savedHighScore);
        Lifecycle lifecycle{renderer, game, scorePath, savedHighScore};
        const LifecycleWatch watch(lifecycle);
        audio::Audio audio;
        app::Controls controls;
        app::FrameStats stats;
        // GALAXIANS_STATS=1 starts with the frame-pacing overlay on and logs it each second.
        const bool logStats = SDL_getenv("GALAXIANS_STATS") != nullptr;
        bool showStats = logStats;
        std::vector<game::Sound> sounds;
        std::vector<gfx::Sprite> sprites;
        std::vector<gfx::Sprite> overlay;
        gfx::StarfieldState stars;

        double interval = refreshInterval(window);
        if (logStats) {
            const SDL_DisplayID display = SDL_GetDisplayForWindow(window);
            const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(display);
            SDL_Log("Display: %s at %.2f Hz", SDL_GetDisplayName(display), mode ? mode->refresh_rate : 0.0f);
        }
        const Uint64 start = SDL_GetPerformanceCounter();
        const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
        double previous = 0.0;
        double accumulator = 0.0;
        bool running = true;
        bool minimized = false;

        while (running) {
            SDL_Event event;
            // Block while minimised instead of spinning. (On Android SDL blocks
            // inside the poll while the app is in the background.)
            if (minimized && SDL_WaitEvent(&event))
                SDL_PushEvent(&event);

            while (SDL_PollEvent(&event)) {
                controls.handleEvent(event);
                switch (event.type) {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (event.key.repeat)
                        break;
                    if (event.key.key == SDLK_ESCAPE || event.key.key == SDLK_AC_BACK)
                        running = false;
                    else if (event.key.key == SDLK_F11 ||
                             (event.key.key == SDLK_RETURN && (event.key.mod & SDL_KMOD_ALT)))
                        toggleFullscreen(window);
                    else if (event.key.key == SDLK_M)
                        audio.setMuted(!audio.muted());
                    else if (event.key.key == SDLK_C)
                        renderer.setCrtEnabled(!renderer.crtEnabled());
                    else if (event.key.key == SDLK_F3)
                        showStats = !showStats;
                    break;
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                    renderer.onResize();
                    break;
                case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
                    SDL_HideCursor();
                    interval = refreshInterval(window);
                    break;
                case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
                    SDL_ShowCursor();
                    interval = refreshInterval(window);
                    break;
                case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
                    interval = refreshInterval(window);
                    break;
                case SDL_EVENT_WINDOW_MINIMIZED:
                    minimized = true;
                    break;
                case SDL_EVENT_WINDOW_RESTORED:
                    minimized = false;
                    renderer.onResize();
                    break;
                default:
                    break;
                }
            }
            if (!running)
                break;
            if (lifecycle.background)
                continue;

            const double now = static_cast<double>(SDL_GetPerformanceCounter() - start) / freq;
            // Carry on from where the app left off, without catching up on the time away.
            if (lifecycle.resumed) {
                previous = now;
                accumulator = 0.0;
                lifecycle.resumed = false;
            }
            const double measured = now - previous;
            previous = now;
            const double frameTime = std::min(snapToVsync(measured, interval), kMaxFrameSeconds);

            if (kAndroid) {
                const app::ScreenLayout layout = layoutWindow(window);
                controls.touch().setLayout(layout);
                renderer.setGameViewport(layout.game.x, layout.game.y, layout.game.w, layout.game.h);
            }

            accumulator += frameTime;
            int ticks = 0;
            while (accumulator >= kTickSeconds) {
                game.update(controls.read());
                accumulator -= kTickSeconds;
                ++ticks;
            }

            if (stats.addFrame(measured, ticks) && logStats) {
                const app::FrameStats::Summary& s = stats.last();
                SDL_Log("frames %d, %.2f-%.2f ms, ticks per frame 0:%d 1:%d 2+:%d", s.frames, s.minMs, s.maxMs,
                        s.framesByTicks[0], s.framesByTicks[1], s.framesByTicks[2]);
            }

            sounds.clear();
            game.takeSounds(sounds);
            for (game::Sound sound : sounds)
                audio.play(sound);
            audio.setHum(game.humActive());

            // Save a new record once the game that set it is over.
            if (game.highScore() != savedHighScore && game.mode() != game::Mode::Playing && !scorePath.empty()) {
                savedHighScore = game.highScore();
                saveHighScore(scorePath, savedHighScore);
            }

            if (!minimized) {
                game.render(sprites, stars, float(accumulator / kTickSeconds));
                if (showStats)
                    stats.draw(sprites);
                overlay.clear();
                if (kAndroid)
                    controls.touch().draw(overlay);
                renderer.drawFrame(stars, sprites, overlay);
            }
        }

        if (game.highScore() != savedHighScore && !scorePath.empty())
            saveHighScore(scorePath, game.highScore());
    } catch (const std::exception& e) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Fatal: %s", e.what());
        status = 1;
    }

    SDL_DestroyWindow(window);
    SDL_Quit();
    return status;
}

} // namespace

int main(int, char**)
{
    return run();
}
