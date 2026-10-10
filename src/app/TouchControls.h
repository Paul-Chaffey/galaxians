#pragma once

#include "app/ScreenLayout.h"
#include "game/World.h"
#include "gfx/Sprite.h"

#include <SDL3/SDL_events.h>

#include <array>
#include <vector>

namespace app {

// On-screen Left, Right and Fire buttons. Each finger presses whichever button
// it is over, so you can move and fire at once and slide between Left and Right.
// The buttons are shown after a touch and hidden once a gamepad or keyboard is
// used, so they stay out of the way of a Bluetooth controller.
class TouchControls {
public:
    // Hit tests use the layout from the most recent call.
    void setLayout(const ScreenLayout& layout) { layout_ = layout; }
    const ScreenLayout& layout() const { return layout_; }

    void handleEvent(const SDL_Event& event);

    // ORs the buttons held now, or pressed since the last call, into `input`.
    // A quick tap can start and end between two game ticks and would otherwise be missed.
    void read(game::Input& input);

    bool visible() const { return visible_; }

    // Appends the buttons, in window pixels, brighter while held.
    void draw(std::vector<gfx::Sprite>& out) const;

private:
    struct Finger {
        SDL_FingerID id;
        int button; // TouchButton, or -1 when off every button
    };

    void press(SDL_FingerID id, float x, float y);
    void release(SDL_FingerID id);
    bool held(TouchButton button) const;

    ScreenLayout layout_;
    std::vector<Finger> fingers_;
    std::array<bool, kTouchButtonCount> tapped_{};
    bool visible_ = true;
};

} // namespace app
