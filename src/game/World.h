#pragma once

#include "game/Sound.h"
#include "gfx/Sprite.h"
#include "gfx/Starfield.h"

#include <cstdint>
#include <vector>

namespace game {

// Controls sampled once per tick, from the keyboard or the demo pilot.
struct Input {
    bool left = false;
    bool right = false;
    bool fire = false;
    bool start = false; // ignored by World; Game uses it to begin a game
};

enum class AlienType { Blue, Purple, Red, Flagship };
enum class AlienState { Formation, PeelOff, Attack, Returning };

// Player geometry, shared with the demo pilot.
inline constexpr float kScreenWidth = 224.0f;
inline constexpr float kPlayerY = 228.0f;
inline constexpr float kPlayerSpeed = 1.5f; // pixels per tick
inline constexpr float kShotSpeed = 4.0f;

// Points for shooting an alien sitting in the formation.
int formationPoints(AlienType type);

// Points for shooting an alien that is out of the formation. A flagship is
// worth more when it brought escorts, and most once every escort is shot.
int divingPoints(AlienType type, int escortsLaunched, int escortsShot);

struct WorldOptions {
    bool aliensAttack = true; // false keeps every alien in formation (for tests)
    uint32_t seed = 0x6A1A7;  // the same seed and inputs always replay the same game
    int highScore = 5000;
    bool recordsHighScore = true; // false for demo games, which must not set records
};

// The game state, advanced in fixed 60 Hz ticks. All positions are sprite
// centres in virtual-screen pixels (224x256, y down).
class World {
public:
    struct Alien {
        AlienType type;
        int row;
        int col;
        bool alive = true;
        AlienState state = AlienState::Formation;
        float x = 0, y = 0;
        float prevX = 0, prevY = 0; // position at the previous tick, for smooth drawing
        float vx = 0, vy = 0;       // only meaningful while attacking
        float rotation = 0;

        // Peel-off: a half circle out of the formation, ending pointed down.
        float loopX = 0, loopY = 0, loopAngle = 0, loopSwept = 0;
        int loopDir = 1;

        // Escorts follow a flagship; flagships count how their escorts fare.
        int leader = -1;
        float escortOffset = 0;
        int escortsLaunched = 0;
        int escortsShot = 0;

        int shotsLeft = 0;
        float nextShotY = 0;
    };

    struct Bullet {
        float x, y, vx, vy;
    };

    explicit World(const WorldOptions& options = {});

    void update(const Input& input);
    // `alpha` in [0, 1] is how far time has moved from the last tick towards
    // the next; positions are blended from the previous tick by that much, so
    // motion stays smooth on displays that aren't 60 Hz.
    void render(std::vector<gfx::Sprite>& sprites, gfx::StarfieldState& stars, float alpha = 1.0f) const;

    int score() const { return score_; }
    int highScore() const { return highScore_; }
    int wave() const { return wave_; }
    int reserveShips() const { return reserveShips_; }
    bool playerAlive() const { return playerAlive_; }
    bool gameOver() const { return gameOver_; }
    int aliensRemaining() const;
    int aliensDiving() const;

    // Read-only views for the demo pilot. Alien indices are stable within a wave.
    const std::vector<Alien>& aliens() const { return aliens_; }
    const std::vector<Bullet>& bullets() const { return bullets_; }
    float playerX() const { return playerX_; }
    bool shotLoaded() const { return playerAlive_ && !shot_.flying; }
    // Sound effects triggered by the most recent update().
    const std::vector<Sound>& sounds() const { return sounds_; }
    // Where a loaded shot sits, and so where a fired one starts.
    static float shotStartY();

private:
    struct Explosion {
        float x, y;
        bool player = false;
        int age = 0;
    };

    struct Shot {
        bool flying = false;
        float x = 0;
        float y = 0;
    };

    struct Popup {
        float x, y;
        int points;
        int age = 0;
    };

    // xorshift32: tiny and identical on every platform.
    struct Rng {
        uint32_t state;
        uint32_t next();
        int below(int n) { return int(next() % uint32_t(n)); }
    };

    void spawnWave();
    void updatePlayer(const Input& input);
    void updateShot(bool firePressed);
    void updateFormation();
    void updateLaunches();
    bool launchFlagship();
    bool launchFromEdge();
    void launch(int index, int dir);
    void updateAliens();
    void updateAttack(int index);
    void updateBullets();
    void resolveShotHits();
    void resolvePlayerHits();
    void destroyAlien(int index);
    void killPlayer();
    void updatePlayerLife();
    void updateEffects();
    void updateWaveProgress();

    void rememberPositions();
    float shotDrawX() const;
    float shotDrawY() const;
    float slotX(const Alien& alien) const;
    float slotY(const Alien& alien) const;

    // Difficulty for the current wave.
    int maxDivers() const;
    int launchInterval();
    float diveSpeed() const;
    int shotsPerDive() const;

    WorldOptions options_;
    Rng rng_;
    uint64_t tick_ = 0;
    bool fireHeld_ = false;

    float playerX_;
    float prevPlayerX_;
    float prevShotX_ = 0, prevShotY_ = 0;
    bool playerAlive_ = true;
    int respawnTimer_ = 0;
    bool gameOver_ = false;
    Shot shot_;

    std::vector<Alien> aliens_;
    int formationOffset_ = 0;
    int formationDir_ = 1;
    int formationBob_ = 0; // vertical offset, pixels
    int launchTimer_ = 0;

    std::vector<Bullet> bullets_;
    std::vector<Explosion> explosions_;
    std::vector<Popup> popups_;
    std::vector<Sound> sounds_;

    int score_ = 0;
    int highScore_;
    int wave_ = 1;
    int reserveShips_ = 2;
    int nextWaveTimer_ = 0;
};

} // namespace game
