#pragma once

#include "game/World.h"

#include <vector>

namespace game {

// Plays the game in attract mode. It reads the same world a player sees and
// answers with the same Input, so the demo obeys every rule a human does.
class DemoPilot {
public:
    Input update(const World& world);

private:
    // Velocity estimated from successive positions, since formation and
    // peel-off motion is not exposed as a velocity.
    struct Track {
        float x = 0, y = 0;
        float vx = 0, vy = 0;
        bool valid = false;
    };

    // Something that will cross the player's line at `x` in `ticks` ticks.
    struct Threat {
        float x;
        float ticks;
        float radius;
    };

    void track(const World& world);
    std::vector<Threat> findThreats(const World& world) const;
    bool chooseTarget(const World& world, float& aimX) const;
    float pathDanger(float from, float to, const std::vector<Threat>& threats) const;

    std::vector<Track> tracks_;
    bool fireHeld_ = false;
};

} // namespace game
