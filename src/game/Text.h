#pragma once

#include "game/Atlas.h"
#include "gfx/Sprite.h"

#include <string_view>
#include <vector>

namespace game {

// Position of the n-th column or row on the arcade's 28x32 character grid.
constexpr float cell(int n)
{
    return float(n) * atlas::kGlyphSize;
}

// Appends one sprite per visible character, starting with the 8x8 cell whose
// top-left is (x, y). Lowercase is drawn as uppercase; characters missing from
// the font are drawn as spaces. `scale` enlarges each pixel into a block.
void drawText(std::vector<gfx::Sprite>& out, std::string_view text, float x, float y,
              uint32_t tint = gfx::rgba(255, 255, 255), int scale = 1);

// As drawText, horizontally centred on the screen.
void drawCentredText(std::vector<gfx::Sprite>& out, std::string_view text, float y,
                     uint32_t tint = gfx::rgba(255, 255, 255), int scale = 1);

} // namespace game
