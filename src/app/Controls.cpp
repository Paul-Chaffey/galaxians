#include "app/Controls.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_log.h>

#include <algorithm>

namespace app {

namespace {

constexpr Sint16 kStickDeadzone = 12000; // of 32767

} // namespace

Controls::~Controls()
{
    for (SDL_Gamepad* pad : gamepads_)
        SDL_CloseGamepad(pad);
}

void Controls::handleEvent(const SDL_Event& event)
{
    touch_.handleEvent(event);

    // SDL also sends GAMEPAD_ADDED for pads already connected at startup.
    if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
        if (SDL_Gamepad* pad = SDL_OpenGamepad(event.gdevice.which)) {
            gamepads_.push_back(pad);
            SDL_Log("Gamepad connected: %s", SDL_GetGamepadName(pad));
        }
    } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
        auto it = std::find_if(gamepads_.begin(), gamepads_.end(), [&](SDL_Gamepad* pad) {
            return SDL_GetGamepadID(pad) == event.gdevice.which;
        });
        if (it != gamepads_.end()) {
            SDL_Log("Gamepad disconnected: %s", SDL_GetGamepadName(*it));
            SDL_CloseGamepad(*it);
            gamepads_.erase(it);
        }
    }
}

game::Input Controls::read()
{
    const bool* keys = SDL_GetKeyboardState(nullptr);
    // Alt+Enter toggles fullscreen, so it must not also start a game.
    const bool alt = (SDL_GetModState() & SDL_KMOD_ALT) != 0;

    game::Input input;
    input.left = keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A];
    input.right = keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];
    input.fire = keys[SDL_SCANCODE_SPACE] || keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_LCTRL];
    input.start = (!alt && (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER])) || keys[SDL_SCANCODE_1];

    for (SDL_Gamepad* pad : gamepads_) {
        const Sint16 stick = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
        input.left |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT) || stick < -kStickDeadzone;
        input.right |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT) || stick > kStickDeadzone;
        input.fire |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH) ||
                      SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST) ||
                      SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_WEST) ||
                      SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_NORTH);
        input.start |= SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_START);
    }
    touch_.read(input);
    return input;
}

} // namespace app
