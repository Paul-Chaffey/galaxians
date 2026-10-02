#pragma once

#include "gfx/Sprite.h"

#include <vector>

namespace game {

struct HudState {
    int score = 0;
    int highScore = 0;
    int reserveShips = 0; // ships waiting at the bottom left, not counting the one in play
    int wave = 1;         // one flag per wave at the bottom right
    bool showPlayerLabel = true; // "1UP" flashes while the player is in control
};

// Appends the score header and the bottom-row lives and wave flags.
void drawHud(std::vector<gfx::Sprite>& out, const HudState& hud);

} // namespace game
