#include "game/DemoPilot.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace game {

namespace {

constexpr float kLookaheadTicks = 70.0f;
constexpr float kBulletRadius = 8.0f; // ship half-width plus a safety margin
constexpr float kDiverRadius = 14.0f;
constexpr float kDiverUncertainty = 0.1f; // divers weave, so widen far-off predictions
constexpr float kMinX = 12.0f;
constexpr float kMaxX = kScreenWidth - 12.0f;
constexpr float kFireTolerance = 2.5f;
constexpr float kMaxTrackStep = 4.0f; // bigger jumps are teleports, not motion

} // namespace

void DemoPilot::track(const World& world)
{
    const auto& aliens = world.aliens();
    if (tracks_.size() != aliens.size())
        tracks_.assign(aliens.size(), {});

    for (size_t i = 0; i < aliens.size(); ++i) {
        const World::Alien& alien = aliens[i];
        Track& t = tracks_[i];
        if (!alien.alive) {
            t.valid = false;
            continue;
        }
        const float dx = alien.x - t.x;
        const float dy = alien.y - t.y;
        if (t.valid && std::abs(dx) <= kMaxTrackStep && std::abs(dy) <= kMaxTrackStep) {
            // Smoothed, because formation sway moves one pixel every other tick.
            t.vx = 0.5f * t.vx + 0.5f * dx;
            t.vy = 0.5f * t.vy + 0.5f * dy;
        } else {
            t.vx = 0;
            t.vy = 0;
        }
        t.x = alien.x;
        t.y = alien.y;
        t.valid = true;
    }
}

std::vector<DemoPilot::Threat> DemoPilot::findThreats(const World& world) const
{
    std::vector<Threat> threats;

    for (const World::Bullet& b : world.bullets()) {
        if (b.vy <= 0)
            continue;
        const float ticks = (kPlayerY - b.y) / b.vy;
        if (ticks < -3.0f || ticks > kLookaheadTicks)
            continue;
        threats.push_back({b.x + b.vx * std::max(ticks, 0.0f), ticks, kBulletRadius});
    }

    const auto& aliens = world.aliens();
    for (size_t i = 0; i < aliens.size(); ++i) {
        const World::Alien& alien = aliens[i];
        const Track& t = tracks_[i];
        if (!alien.alive || alien.state != AlienState::Attack || t.vy < 0.3f)
            continue;
        const float ticks = (kPlayerY - alien.y) / t.vy;
        if (ticks < -8.0f || ticks > kLookaheadTicks)
            continue;
        const float x = std::clamp(alien.x + t.vx * std::max(ticks, 0.0f), kMinX, kMaxX);
        threats.push_back({x, ticks, kDiverRadius + kDiverUncertainty * std::max(ticks, 0.0f)});
    }
    return threats;
}

bool DemoPilot::chooseTarget(const World& world, float& aimX) const
{
    const float px = world.playerX();
    const float shotY = World::shotStartY();
    float best = -1.0f;

    const auto& aliens = world.aliens();
    for (size_t i = 0; i < aliens.size(); ++i) {
        const World::Alien& alien = aliens[i];
        const Track& t = tracks_[i];
        if (!alien.alive || !t.valid || alien.y < 16.0f || alien.y >= shotY - 8.0f)
            continue;

        // Lead the target: where will it be when a shot fired now reaches it?
        const float closing = kShotSpeed + t.vy;
        if (closing < 0.5f)
            continue;
        const float ticks = (shotY - alien.y) / closing;
        const float x = alien.x + t.vx * ticks;
        if (x < kMinX || x > kMaxX)
            continue;

        // Diving aliens are worth more; nearby ones are quicker to line up.
        const int points = alien.state == AlienState::Formation ? formationPoints(alien.type)
                                                                 : divingPoints(alien.type, 0, 0);
        const float value = float(points) / (20.0f + std::abs(x - px));
        if (value > best) {
            best = value;
            aimX = x;
        }
    }
    return best >= 0.0f;
}

float DemoPilot::pathDanger(float from, float to, const std::vector<Threat>& threats) const
{
    // Judge each threat against where the ship will be when it arrives, so a
    // route that crosses a falling bullet counts as dangerous.
    float danger = 0;
    for (const Threat& threat : threats) {
        const float reach = kPlayerSpeed * std::max(threat.ticks, 0.0f);
        const float shipX = from + std::clamp(to - from, -reach, reach);
        const float distance = std::abs(shipX - threat.x);
        if (distance < threat.radius) {
            const float urgency = 1.0f + (kLookaheadTicks - threat.ticks) / kLookaheadTicks;
            danger += (threat.radius - distance) * urgency;
        }
    }
    return danger;
}

Input DemoPilot::update(const World& world)
{
    track(world);

    Input input;
    if (!world.playerAlive()) {
        fireHeld_ = false;
        return input;
    }

    const float px = world.playerX();
    const std::vector<Threat> threats = findThreats(world);

    float aimX = px;
    const bool haveTarget = chooseTarget(world, aimX);

    // Head for the target unless the way there, or staying there, is unsafe.
    float bestX = px;
    float bestCost = std::numeric_limits<float>::max();
    for (float x = kMinX; x <= kMaxX; x += 1.0f) {
        const float cost = pathDanger(px, x, threats) * 1000.0f + std::abs(x - aimX) + std::abs(x - px) * 0.1f;
        if (cost < bestCost) {
            bestCost = cost;
            bestX = x;
        }
    }
    if (bestX < px - kPlayerSpeed * 0.5f)
        input.left = true;
    else if (bestX > px + kPlayerSpeed * 0.5f)
        input.right = true;

    // Fire on a fresh press when lined up.
    const bool fire = haveTarget && world.shotLoaded() && !fireHeld_ && std::abs(px - aimX) <= kFireTolerance;
    input.fire = fire;
    fireHeld_ = fire;
    return input;
}

} // namespace game
