#include "game/Hud.h"

#include "game/Atlas.h"
#include "game/Text.h"

#include <algorithm>
#include <string>

namespace game {

namespace {

constexpr uint32_t kLabelColor = gfx::rgba(0xE0, 0x20, 0x30);
constexpr uint32_t kScoreColor = gfx::rgba(0xFF, 0xFF, 0xFF);
constexpr int kMaxFlags = 12;

// Scores are always multiples of ten, so zero is shown as "00" like the arcade.
std::string formatScore(int score)
{
    return score == 0 ? "00" : std::to_string(score);
}

// Draws `text` so that its last character sits in column `lastColumn`.
void drawRightAligned(std::vector<gfx::Sprite>& out, const std::string& text, int lastColumn, int row,
                      uint32_t tint)
{
    drawText(out, text, cell(lastColumn + 1 - int(text.size())), cell(row), tint);
}

} // namespace

void drawHud(std::vector<gfx::Sprite>& out, const HudState& hud)
{
    if (hud.showPlayerLabel)
        drawText(out, "1UP", cell(3), cell(0), kLabelColor);
    drawText(out, "HIGH SCORE", cell(9), cell(0), kLabelColor);

    drawRightAligned(out, formatScore(hud.score), 6, 1, kScoreColor);
    drawRightAligned(out, formatScore(hud.highScore), 16, 1, kScoreColor);

    const float bottom = cell(31);
    for (int i = 0; i < hud.reserveShips; ++i)
        out.push_back(gfx::Sprite::at(atlas::kShip, cell(1) + float(i) * 16.0f, bottom + 8.0f - atlas::kShip.h));

    const int flags = std::clamp(hud.wave, 0, kMaxFlags);
    for (int i = 0; i < flags; ++i)
        out.push_back(gfx::Sprite::at(atlas::kFlag, cell(26 - i), bottom));
}

} // namespace game
