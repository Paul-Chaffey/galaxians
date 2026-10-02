#pragma once

#include "gfx/Sprite.h"

#include <vector>

namespace app {

// Frame-pacing measurements over one-second windows: frame rate, the spread
// of frame times, and how many 60 Hz game ticks each frame ran. On a 60 Hz
// display every frame should run exactly one tick; frames with 0 or 2 ticks
// there mean visible stutter.
class FrameStats {
public:
    struct Summary {
        int frames = 0;
        double minMs = 0;
        double maxMs = 0;
        int framesByTicks[3] = {}; // frames that ran 0, 1, and 2+ ticks
    };

    // Returns true when this frame completed a one-second window.
    bool addFrame(double seconds, int ticks);

    const Summary& last() const { return last_; }

    // Appends the last summary as three lines of text at the top-left, between
    // the score and the formation.
    void draw(std::vector<gfx::Sprite>& out) const;

private:
    Summary current_;
    Summary last_;
    double elapsed_ = 0;
};

} // namespace app
