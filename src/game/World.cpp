#include "game/World.h"

#include "game/Atlas.h"
#include "game/Hud.h"
#include "game/Text.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>

namespace game {

namespace {

constexpr float kScreenHeight = 256.0f;
constexpr float kPi = std::numbers::pi_v<float>;

constexpr float kPlayerMinX = 12.0f;
constexpr float kPlayerMaxX = kScreenWidth - 12.0f;
constexpr int kRespawnDelay = 150;

// A loaded shot sits on the ship's nose, as on the arcade machine.
constexpr float kShotRestY = kPlayerY - atlas::kShip.h * 0.5f - atlas::kPlayerShot.h * 0.5f;

constexpr int kColumns = 10;
constexpr float kColumnSpacing = 16.0f;
constexpr float kFormationLeft = 40.0f; // centre of column 0 before sway
constexpr float kRowY[] = {45.0f, 61.0f, 75.0f, 89.0f, 103.0f, 117.0f};
constexpr int kSwayTicksPerPixel = 4; // side to side: 15 pixels per second
constexpr float kSwayMargin = 15.0f; // closest an alien centre gets to a screen edge
constexpr float kBobAmplitude = 2.0f; // up and down: +/- pixels
constexpr int kBobPeriodTicks = 240;  // one full bob every four seconds

// Diving.
constexpr float kLoopRadius = 12.0f;
constexpr float kLoopSpeed = 0.11f;  // radians per tick
constexpr float kSteerGain = 0.02f;  // how hard divers home in on their target
constexpr float kMaxSteer = 0.08f;
constexpr float kMaxDiveVx = 1.6f;
constexpr float kDiveMinX = 8.0f;
constexpr float kDiveMaxX = kScreenWidth - 8.0f;
constexpr float kEscortSpacing = 12.0f;
constexpr float kReturnSpeed = 1.0f;
constexpr float kOffscreenMargin = 12.0f;
constexpr int kWaveStartDelay = 150;

// Alien fire.
constexpr float kBulletSpeed = 2.0f;
constexpr float kMaxBulletVx = 0.6f;
constexpr float kFirstShotY = 100.0f;
constexpr float kLastShotY = kPlayerY - 50.0f;

// Hitbox half-sizes, a little inside the drawn sprites.
constexpr float kAlienHalfWidth = 6.0f;
constexpr float kAlienHalfHeight = 5.0f;
constexpr float kPlayerHalfWidth = 4.0f;
constexpr float kPlayerHalfHeight = 6.0f;
constexpr float kRamHalfWidth = 10.0f; // alien body against the ship
constexpr float kRamHalfHeight = 8.0f;

constexpr int kExplosionTicksPerFrame = 6;
constexpr gfx::AtlasRect kExplosionFrames[] = {
    atlas::kExplosion1, atlas::kExplosion2, atlas::kExplosion3, atlas::kExplosion4};
constexpr int kPlayerExplosionTicksPerFrame = 12;
constexpr gfx::AtlasRect kPlayerExplosionFrames[] = {
    atlas::kPlayerExplosion1, atlas::kPlayerExplosion2, atlas::kPlayerExplosion3};
constexpr int kPopupTicks = 90;
constexpr uint32_t kPopupColor = gfx::rgba(0x50, 0xD0, 0xFF);

constexpr int kNextWaveDelay = 120;
constexpr float kStarScrollPerTick = 0.5f;

const gfx::AtlasRect& alienFrame(AlienType type, bool wingsUp)
{
    switch (type) {
    case AlienType::Blue: return wingsUp ? atlas::kBlueAlienA : atlas::kBlueAlienB;
    case AlienType::Purple: return wingsUp ? atlas::kPurpleAlienA : atlas::kPurpleAlienB;
    case AlienType::Red: return wingsUp ? atlas::kRedAlienA : atlas::kRedAlienB;
    case AlienType::Flagship: return atlas::kFlagship;
    }
    return atlas::kBlueAlienA;
}

gfx::Sprite centred(const gfx::AtlasRect& rect, float x, float y)
{
    return gfx::Sprite::at(rect, x - rect.w * 0.5f, y - rect.h * 0.5f);
}

// Blends from the previous tick's position, except across teleports such as
// a diver re-entering at the top of the screen.
float blend(float previous, float current, float alpha)
{
    constexpr float kTeleport = 8.0f;
    if (std::abs(current - previous) > kTeleport)
        return current;
    return previous + (current - previous) * alpha;
}

// Sprites face down at rotation 0; this turns them to face along (vx, vy).
float facing(float vx, float vy)
{
    return std::atan2(-vx, vy);
}

} // namespace

int formationPoints(AlienType type)
{
    switch (type) {
    case AlienType::Blue: return 30;
    case AlienType::Purple: return 40;
    case AlienType::Red: return 50;
    case AlienType::Flagship: return 60;
    }
    return 0;
}

int divingPoints(AlienType type, int escortsLaunched, int escortsShot)
{
    switch (type) {
    case AlienType::Blue: return 60;
    case AlienType::Purple: return 80;
    case AlienType::Red: return 100;
    case AlienType::Flagship:
        if (escortsLaunched == 0)
            return 150;
        if (escortsShot == 0)
            return 200;
        if (escortsLaunched == 2 && escortsShot == 2)
            return 800;
        return 300;
    }
    return 0;
}

uint32_t World::Rng::next()
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

World::World(const WorldOptions& options)
    : options_(options)
    , rng_{options.seed != 0 ? options.seed : 1}
    , playerX_(kScreenWidth * 0.5f)
    , prevPlayerX_(playerX_)
    , highScore_(options.highScore)
{
    spawnWave();
    rememberPositions();
}

float World::shotStartY()
{
    return kShotRestY;
}

void World::spawnWave()
{
    aliens_.clear();
    for (int col : {3, 6})
        aliens_.push_back({AlienType::Flagship, 0, col});
    for (int col = 2; col <= 7; ++col)
        aliens_.push_back({AlienType::Red, 1, col});
    for (int col = 1; col <= 8; ++col)
        aliens_.push_back({AlienType::Purple, 2, col});
    for (int row = 3; row <= 5; ++row)
        for (int col = 0; col < kColumns; ++col)
            aliens_.push_back({AlienType::Blue, row, col});

    formationOffset_ = 0;
    formationDir_ = 1;
    formationBob_ = 0;
    for (Alien& alien : aliens_) {
        alien.x = alien.prevX = slotX(alien);
        alien.y = alien.prevY = slotY(alien);
    }
    bullets_.clear();
    launchTimer_ = kWaveStartDelay;
}

int World::aliensRemaining() const
{
    return int(std::count_if(aliens_.begin(), aliens_.end(), [](const Alien& a) { return a.alive; }));
}

int World::aliensDiving() const
{
    return int(std::count_if(aliens_.begin(), aliens_.end(), [](const Alien& a) {
        return a.alive && a.state != AlienState::Formation;
    }));
}

float World::slotX(const Alien& alien) const
{
    return kFormationLeft + float(alien.col) * kColumnSpacing + float(formationOffset_);
}

float World::slotY(const Alien& alien) const
{
    return kRowY[alien.row] + float(formationBob_);
}

int World::maxDivers() const
{
    return std::min(1 + wave_, 6);
}

int World::launchInterval()
{
    const int interval = std::max(40, 140 - 15 * (wave_ - 1)) + rng_.below(40);
    // The last few aliens attack relentlessly.
    return aliensRemaining() <= 8 ? interval / 2 : interval;
}

float World::diveSpeed() const
{
    return std::min(1.1f + 0.1f * float(wave_ - 1), 1.9f);
}

int World::shotsPerDive() const
{
    return std::min(1 + wave_ / 2, 4);
}

void World::update(const Input& input)
{
    ++tick_;
    sounds_.clear();
    rememberPositions();
    const bool firePressed = input.fire && !fireHeld_;
    fireHeld_ = input.fire;

    if (playerAlive_)
        updatePlayer(input);
    updateShot(firePressed && playerAlive_);
    updateFormation();
    updateLaunches();
    updateAliens();
    updateBullets();
    resolveShotHits();
    if (playerAlive_)
        resolvePlayerHits();
    updateEffects();
    updatePlayerLife();
    updateWaveProgress();
}

void World::rememberPositions()
{
    for (Alien& alien : aliens_) {
        alien.prevX = alien.x;
        alien.prevY = alien.y;
    }
    prevPlayerX_ = playerX_;
    prevShotX_ = shotDrawX();
    prevShotY_ = shotDrawY();
}

// Where the shot is drawn: in flight, or loaded on the ship's nose.
float World::shotDrawX() const
{
    return shot_.flying ? shot_.x : playerX_;
}

float World::shotDrawY() const
{
    return shot_.flying ? shot_.y : kShotRestY;
}

void World::updatePlayer(const Input& input)
{
    if (input.left != input.right)
        playerX_ += input.left ? -kPlayerSpeed : kPlayerSpeed;
    playerX_ = std::clamp(playerX_, kPlayerMinX, kPlayerMaxX);
}

void World::updateShot(bool firePressed)
{
    // Only one shot on screen at a time.
    if (!shot_.flying) {
        if (firePressed) {
            shot_.flying = true;
            shot_.x = playerX_;
            shot_.y = kShotRestY;
            sounds_.push_back(Sound::PlayerShot);
        }
        return;
    }

    shot_.y -= kShotSpeed;
    if (shot_.y < -atlas::kPlayerShot.h)
        shot_.flying = false;
}

void World::updateFormation()
{
    // A gentle bob, in whole pixels so the formation never smears.
    const float phase = 2.0f * kPi * float(tick_ % kBobPeriodTicks) / float(kBobPeriodTicks);
    formationBob_ = int(std::lround(kBobAmplitude * std::sin(phase)));

    if (tick_ % kSwayTicksPerPixel != 0)
        return;

    // Bounce off the screen edges using the outermost surviving columns, so the
    // formation roams wider as its edges are shot away.
    int minCol = kColumns;
    int maxCol = -1;
    for (const Alien& alien : aliens_) {
        if (!alien.alive)
            continue;
        minCol = std::min(minCol, alien.col);
        maxCol = std::max(maxCol, alien.col);
    }
    if (maxCol < 0)
        return;

    const float left = kFormationLeft + float(minCol) * kColumnSpacing + float(formationOffset_);
    const float right = kFormationLeft + float(maxCol) * kColumnSpacing + float(formationOffset_);
    if (formationDir_ < 0 && left <= kSwayMargin)
        formationDir_ = 1;
    else if (formationDir_ > 0 && right >= kScreenWidth - kSwayMargin)
        formationDir_ = -1;
    formationOffset_ += formationDir_;
}

void World::updateLaunches()
{
    if (!options_.aliensAttack || !playerAlive_ || nextWaveTimer_ > 0)
        return;
    if (launchTimer_ > 0) {
        --launchTimer_;
        return;
    }
    launchTimer_ = launchInterval();
    if (aliensDiving() >= maxDivers())
        return;

    if (rng_.below(3) == 0 && launchFlagship())
        return;
    launchFromEdge();
}

bool World::launchFlagship()
{
    std::vector<int> flagships;
    for (int i = 0; i < int(aliens_.size()); ++i)
        if (aliens_[i].alive && aliens_[i].type == AlienType::Flagship && aliens_[i].state == AlienState::Formation)
            flagships.push_back(i);
    if (flagships.empty())
        return false;

    const int leader = flagships[rng_.below(int(flagships.size()))];
    const int dir = aliens_[leader].col < kColumns / 2 ? -1 : 1;
    launch(leader, dir);

    // Up to two red aliens from just below the flagship fly as escorts.
    int escorts = 0;
    for (int i = 0; i < int(aliens_.size()) && escorts < 2; ++i) {
        Alien& alien = aliens_[i];
        if (alien.alive && alien.type == AlienType::Red && alien.state == AlienState::Formation &&
            std::abs(alien.col - aliens_[leader].col) <= 1) {
            launch(i, dir);
            alien.leader = leader;
            alien.escortOffset = escorts == 0 ? -kEscortSpacing : kEscortSpacing;
            ++escorts;
        }
    }
    aliens_[leader].escortsLaunched = escorts;
    sounds_.push_back(Sound::Dive);
    return true;
}

bool World::launchFromEdge()
{
    // Divers come from the outermost alien of each row on one side.
    const int side = rng_.below(2) == 0 ? -1 : 1;
    std::vector<int> candidates;
    for (int row = 1; row <= 5; ++row) {
        int best = -1;
        for (int i = 0; i < int(aliens_.size()); ++i) {
            const Alien& alien = aliens_[i];
            if (!alien.alive || alien.row != row || alien.state != AlienState::Formation)
                continue;
            if (best < 0 || (side < 0 ? alien.col < aliens_[best].col : alien.col > aliens_[best].col))
                best = i;
        }
        if (best >= 0)
            candidates.push_back(best);
    }
    if (candidates.empty())
        return false;

    const int index = candidates[rng_.below(int(candidates.size()))];
    launch(index, side);
    sounds_.push_back(Sound::Dive);
    return true;
}

void World::launch(int index, int dir)
{
    Alien& alien = aliens_[index];
    alien.state = AlienState::PeelOff;
    alien.loopDir = dir;
    alien.loopX = alien.x + float(dir) * kLoopRadius;
    alien.loopY = alien.y;
    alien.loopAngle = dir < 0 ? 0.0f : kPi;
    alien.loopSwept = 0;
    alien.vx = 0;
    alien.vy = 0;
    alien.leader = -1;
    alien.escortsLaunched = 0;
    alien.escortsShot = 0;
    alien.shotsLeft = shotsPerDive();
    alien.nextShotY = kFirstShotY + float(rng_.below(40));
}

void World::updateAliens()
{
    for (int i = 0; i < int(aliens_.size()); ++i) {
        Alien& alien = aliens_[i];
        if (!alien.alive)
            continue;

        switch (alien.state) {
        case AlienState::Formation:
            alien.x = slotX(alien);
            alien.y = slotY(alien);
            alien.rotation = 0;
            break;

        case AlienState::PeelOff: {
            alien.loopAngle += float(alien.loopDir) * kLoopSpeed;
            alien.loopSwept += kLoopSpeed;
            alien.x = alien.loopX + kLoopRadius * std::cos(alien.loopAngle);
            alien.y = alien.loopY + kLoopRadius * std::sin(alien.loopAngle);
            const float dx = -float(alien.loopDir) * std::sin(alien.loopAngle);
            const float dy = float(alien.loopDir) * std::cos(alien.loopAngle);
            alien.rotation = facing(dx, dy);
            if (alien.loopSwept >= kPi) {
                alien.state = AlienState::Attack;
                alien.vx = 0;
                alien.vy = diveSpeed();
            }
            break;
        }

        case AlienState::Attack:
            updateAttack(i);
            break;

        case AlienState::Returning:
            alien.x = slotX(alien);
            alien.y += kReturnSpeed;
            alien.rotation = 0;
            if (alien.y >= slotY(alien)) {
                alien.y = slotY(alien);
                alien.state = AlienState::Formation;
            }
            break;
        }
    }
}

void World::updateAttack(int index)
{
    Alien& alien = aliens_[index];

    // Escorts lose formation once their flagship is gone or home.
    if (alien.leader >= 0) {
        const Alien& leader = aliens_[alien.leader];
        if (!leader.alive || leader.state == AlienState::Formation || leader.state == AlienState::Returning)
            alien.leader = -1;
    }

    float target = playerAlive_ ? playerX_ : kScreenWidth * 0.5f;
    if (alien.leader >= 0)
        target = aliens_[alien.leader].x + alien.escortOffset;

    // Spring-like steering without damping overshoots, giving the weaving dive.
    const float steer = std::clamp((target - alien.x) * kSteerGain, -kMaxSteer, kMaxSteer);
    alien.vx = std::clamp(alien.vx + steer, -kMaxDiveVx, kMaxDiveVx);
    alien.x = std::clamp(alien.x + alien.vx, kDiveMinX, kDiveMaxX);
    alien.y += alien.vy;
    alien.rotation = facing(alien.vx, alien.vy);

    if (playerAlive_ && alien.shotsLeft > 0 && alien.y >= alien.nextShotY && alien.y < kLastShotY) {
        const float ticksToPlayer = (kPlayerY - alien.y) / kBulletSpeed;
        const float vx = std::clamp((playerX_ - alien.x) / ticksToPlayer, -kMaxBulletVx, kMaxBulletVx);
        bullets_.push_back({alien.x, alien.y + 6.0f, vx, kBulletSpeed});
        --alien.shotsLeft;
        alien.nextShotY = alien.y + 16.0f + float(rng_.below(16));
    }

    // Off the bottom: come back in from the top and drop into the slot.
    if (alien.y > kScreenHeight + kOffscreenMargin) {
        alien.state = AlienState::Returning;
        alien.y = -kOffscreenMargin;
        alien.leader = -1;
    }
}

void World::updateBullets()
{
    for (Bullet& b : bullets_) {
        b.x += b.vx;
        b.y += b.vy;
    }
    std::erase_if(bullets_, [](const Bullet& b) { return b.y > kScreenHeight + 4.0f; });
}

void World::resolveShotHits()
{
    if (!shot_.flying)
        return;

    const float shotHalfHeight = atlas::kPlayerShot.h * 0.5f;
    for (int i = 0; i < int(aliens_.size()); ++i) {
        const Alien& alien = aliens_[i];
        if (!alien.alive)
            continue;
        if (std::abs(shot_.x - alien.x) <= kAlienHalfWidth &&
            std::abs(shot_.y - alien.y) <= kAlienHalfHeight + shotHalfHeight) {
            shot_.flying = false;
            destroyAlien(i);
            return;
        }
    }
}

void World::resolvePlayerHits()
{
    for (const Bullet& b : bullets_) {
        if (std::abs(b.x - playerX_) <= kPlayerHalfWidth && std::abs(b.y - kPlayerY) <= kPlayerHalfHeight) {
            killPlayer();
            return;
        }
    }

    for (int i = 0; i < int(aliens_.size()); ++i) {
        const Alien& alien = aliens_[i];
        if (!alien.alive || alien.state == AlienState::Formation)
            continue;
        if (std::abs(alien.x - playerX_) <= kRamHalfWidth && std::abs(alien.y - kPlayerY) <= kRamHalfHeight) {
            destroyAlien(i);
            killPlayer();
            return;
        }
    }
}

void World::destroyAlien(int index)
{
    Alien& alien = aliens_[index];
    alien.alive = false;
    explosions_.push_back({alien.x, alien.y});
    sounds_.push_back(alien.type == AlienType::Flagship ? Sound::FlagshipExplosion : Sound::AlienExplosion);

    int points = formationPoints(alien.type);
    if (alien.state != AlienState::Formation) {
        points = divingPoints(alien.type, alien.escortsLaunched, alien.escortsShot);
        if (alien.leader >= 0) {
            Alien& leader = aliens_[alien.leader];
            if (leader.alive && leader.state != AlienState::Formation)
                ++leader.escortsShot;
        }
        if (alien.type == AlienType::Flagship)
            popups_.push_back({alien.x, alien.y, points});
    }

    score_ += points;
    if (options_.recordsHighScore)
        highScore_ = std::max(highScore_, score_);
}

void World::killPlayer()
{
    playerAlive_ = false;
    explosions_.push_back({playerX_, kPlayerY, true});
    sounds_.push_back(Sound::PlayerExplosion);
    shot_.flying = false;
    respawnTimer_ = kRespawnDelay;
}

void World::updatePlayerLife()
{
    if (playerAlive_ || gameOver_)
        return;
    if (respawnTimer_ > 0) {
        --respawnTimer_;
        return;
    }
    if (reserveShips_ == 0) {
        gameOver_ = true;
        return;
    }
    // Like the arcade, the next ship waits until every diver is back home.
    if (aliensDiving() > 0)
        return;

    --reserveShips_;
    playerAlive_ = true;
    playerX_ = prevPlayerX_ = kScreenWidth * 0.5f;
    bullets_.clear();
    launchTimer_ = kWaveStartDelay;
}

void World::updateEffects()
{
    for (Explosion& e : explosions_)
        ++e.age;
    std::erase_if(explosions_, [](const Explosion& e) {
        return e.age >= (e.player ? kPlayerExplosionTicksPerFrame * 3 : kExplosionTicksPerFrame * 4);
    });

    for (Popup& p : popups_)
        ++p.age;
    std::erase_if(popups_, [](const Popup& p) { return p.age >= kPopupTicks; });
}

void World::updateWaveProgress()
{
    if (nextWaveTimer_ > 0) {
        if (--nextWaveTimer_ == 0) {
            ++wave_;
            spawnWave();
        }
        return;
    }
    if (aliensRemaining() == 0)
        nextWaveTimer_ = kNextWaveDelay;
}

void World::render(std::vector<gfx::Sprite>& out, gfx::StarfieldState& stars, float alpha) const
{
    out.clear();
    alpha = std::clamp(alpha, 0.0f, 1.0f);
    stars.scroll = (float(tick_) - 1.0f + alpha) * kStarScrollPerTick;
    stars.blinkFrame = uint32_t(tick_ / 15);

    for (const Alien& alien : aliens_) {
        if (!alien.alive)
            continue;
        // Wings flap in a ripple across the columns.
        const bool wingsUp = ((tick_ + uint64_t(alien.col) * 4) / 16) % 2 == 0;
        gfx::Sprite sprite = centred(alienFrame(alien.type, wingsUp), blend(alien.prevX, alien.x, alpha),
                                     blend(alien.prevY, alien.y, alpha));
        sprite.rotation = alien.rotation;
        out.push_back(sprite);
    }

    // Bullets fly in straight lines, so their last position is one step back.
    for (const Bullet& b : bullets_)
        out.push_back(centred(atlas::kAlienShot, b.x - b.vx * (1.0f - alpha), b.y - b.vy * (1.0f - alpha)));

    for (const Explosion& e : explosions_) {
        const gfx::AtlasRect& frame = e.player ? kPlayerExplosionFrames[e.age / kPlayerExplosionTicksPerFrame]
                                               : kExplosionFrames[e.age / kExplosionTicksPerFrame];
        out.push_back(centred(frame, e.x, e.y));
    }

    for (const Popup& p : popups_) {
        const std::string text = std::to_string(p.points);
        drawText(out, text, std::floor(p.x - float(text.size()) * atlas::kGlyphSize * 0.5f), p.y - 4.0f,
                 kPopupColor);
    }

    if (playerAlive_ || shot_.flying)
        out.push_back(centred(atlas::kPlayerShot, blend(prevShotX_, shotDrawX(), alpha),
                              blend(prevShotY_, shotDrawY(), alpha)));
    if (playerAlive_)
        out.push_back(centred(atlas::kShip, blend(prevPlayerX_, playerX_, alpha), kPlayerY));

    HudState hud;
    hud.score = score_;
    hud.highScore = highScore_;
    hud.reserveShips = reserveShips_;
    hud.wave = wave_;
    hud.showPlayerLabel = !playerAlive_ || (tick_ / 16) % 2 == 0;
    drawHud(out, hud);
}

} // namespace game
