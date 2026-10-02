#include "app/FrameStats.h"

#include "game/Text.h"

#include <algorithm>
#include <cstdio>

namespace app {

bool FrameStats::addFrame(double seconds, int ticks)
{
    const double ms = seconds * 1000.0;
    if (current_.frames == 0) {
        current_.minMs = ms;
        current_.maxMs = ms;
    }
    ++current_.frames;
    current_.minMs = std::min(current_.minMs, ms);
    current_.maxMs = std::max(current_.maxMs, ms);
    ++current_.framesByTicks[std::min(ticks, 2)];

    elapsed_ += seconds;
    if (elapsed_ < 1.0)
        return false;
    last_ = current_;
    current_ = {};
    elapsed_ = 0;
    return true;
}

void FrameStats::draw(std::vector<gfx::Sprite>& out) const
{
    constexpr uint32_t kColor = gfx::rgba(0x60, 0xFF, 0x60);
    char line[32];

    std::snprintf(line, sizeof(line), "FPS %d", last_.frames);
    game::drawText(out, line, game::cell(0), game::cell(2), kColor);
    std::snprintf(line, sizeof(line), "MS %.1f-%.1f", last_.minMs, last_.maxMs);
    game::drawText(out, line, game::cell(0), game::cell(3), kColor);
    std::snprintf(line, sizeof(line), "TICKS 0:%d 1:%d 2:%d", last_.framesByTicks[0], last_.framesByTicks[1],
                  last_.framesByTicks[2]);
    game::drawText(out, line, game::cell(0), game::cell(4), kColor);
}

} // namespace app
