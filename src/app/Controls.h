#pragma once

#include "app/TouchControls.h"
#include "game/World.h"

#include <SDL3/SDL_events.h>

#include <vector>

struct SDL_Gamepad;

namespace app {

// Merges the keyboard, any connected gamepads and the on-screen buttons into
// one game::Input. Gamepads can be plugged in or removed at any time.
class Controls {
public:
    Controls() = default;
    ~Controls();

    Controls(const Controls&) = delete;
    Controls& operator=(const Controls&) = delete;

    // Opens and closes gamepads as they come and go, and tracks touches.
    void handleEvent(const SDL_Event& event);

    game::Input read();

    // The on-screen buttons; they do nothing until given a layout.
    TouchControls& touch() { return touch_; }

private:
    std::vector<SDL_Gamepad*> gamepads_;
    TouchControls touch_;
};

} // namespace app
