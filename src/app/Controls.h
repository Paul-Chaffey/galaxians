#pragma once

#include "game/World.h"

#include <SDL3/SDL_events.h>

#include <vector>

struct SDL_Gamepad;

namespace app {

// Merges the keyboard and any connected gamepads into one game::Input.
// Gamepads can be plugged in or removed at any time.
class Controls {
public:
    Controls() = default;
    ~Controls();

    Controls(const Controls&) = delete;
    Controls& operator=(const Controls&) = delete;

    // Opens and closes gamepads as they come and go; other events are ignored.
    void handleEvent(const SDL_Event& event);

    game::Input read() const;

private:
    std::vector<SDL_Gamepad*> gamepads_;
};

} // namespace app
