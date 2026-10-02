// Headless checks of the game rules: drives game::World with scripted input.
#include "game/DemoPilot.h"
#include "game/Game.h"
#include "game/World.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace {

int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++failures;
}

constexpr int kTicksPerSecond = 60;

// Sweeps the ship back and forth, tapping fire every other tick.
game::Input sweepAndFire(int tick)
{
    game::Input input;
    const bool goingRight = (tick / 200) % 2 == 0;
    input.right = goingRight;
    input.left = !goingRight;
    input.fire = tick % 2 == 0;
    return input;
}

game::World galleryWorld()
{
    game::WorldOptions options;
    options.aliensAttack = false;
    return game::World(options);
}

void testWaveCanBeCleared()
{
    game::World world = galleryWorld();
    check(world.aliensRemaining() == 46, "wave starts with 46 aliens");

    // The demo pilot aims, so this doesn't depend on lucky timing against the sway.
    game::DemoPilot pilot;
    int tick = 0;
    while (world.aliensRemaining() > 0 && tick++ < 120 * kTicksPerSecond)
        world.update(pilot.update(world));

    check(world.aliensRemaining() == 0, "aimed fire clears a passive wave within two minutes");
    // 30 blue x30 + 8 purple x40 + 6 red x50 + 2 flagships x60.
    check(world.score() == 1640, "clearing the formation scores 1640");

    for (int i = 0; i < 200; ++i)
        world.update({});
    check(world.wave() == 2 && world.aliensRemaining() == 46, "next wave arrives after a pause");
}

void testHoldingFireShootsOnce()
{
    game::World world = galleryWorld();
    game::Input input;
    input.fire = true;
    for (int tick = 0; tick < 30 * kTicksPerSecond; ++tick) {
        input.right = (tick / 200) % 2 == 0;
        input.left = !input.right;
        world.update(input);
    }
    check(world.aliensRemaining() >= 45, "holding fire launches at most one shot");
}

void testDivingPoints()
{
    using game::AlienType;
    check(game::divingPoints(AlienType::Blue, 0, 0) == 60 && game::divingPoints(AlienType::Purple, 0, 0) == 80 &&
              game::divingPoints(AlienType::Red, 0, 0) == 100,
          "diving aliens are worth double their formation value");
    check(game::divingPoints(AlienType::Flagship, 0, 0) == 150, "lone flagship scores 150");
    check(game::divingPoints(AlienType::Flagship, 2, 0) == 200, "escorted flagship scores 200");
    check(game::divingPoints(AlienType::Flagship, 2, 1) == 300, "flagship after one escort scores 300");
    check(game::divingPoints(AlienType::Flagship, 2, 2) == 800, "flagship after both escorts scores 800");
}

void testAliensAttack()
{
    game::World passive = galleryWorld();
    game::World attacking;
    int maxPassive = 0;
    int maxAttacking = 0;
    for (int tick = 0; tick < 20 * kTicksPerSecond; ++tick) {
        passive.update({});
        attacking.update({});
        maxPassive = std::max(maxPassive, passive.aliensDiving());
        maxAttacking = std::max(maxAttacking, attacking.aliensDiving());
    }
    check(maxPassive == 0, "passive aliens never leave the formation");
    check(maxAttacking > 0, "aliens dive within twenty seconds");
}

void testIdlePlayerLosesAllLives()
{
    game::World world;
    int tick = 0;
    bool died = false;
    while (!world.gameOver() && tick < 10 * 60 * kTicksPerSecond) {
        world.update({});
        died = died || !world.playerAlive();
        ++tick;
    }
    check(died, "a motionless player gets shot");
    check(world.gameOver() && world.reserveShips() == 0, "losing every ship ends the game");
}

// Plays one game with the demo pilot; returns ticks survived.
int playWithPilot(uint32_t seed, int& score, int& wave)
{
    game::WorldOptions options;
    options.seed = seed;
    game::World world(options);
    game::DemoPilot pilot;
    int tick = 0;
    while (!world.gameOver() && tick < 10 * 60 * kTicksPerSecond) {
        world.update(pilot.update(world));
        ++tick;
    }
    score = world.score();
    wave = world.wave();
    return tick;
}

void testPilotPlaysWell()
{
    int idleTicks = 0;
    {
        game::World world;
        while (!world.gameOver())
            world.update({}), ++idleTicks;
    }

    int totalTicks = 0;
    int totalScore = 0;
    int bestWave = 0;
    constexpr int kGames = 8;
    uint32_t seed = 1;
    for (int i = 0; i < kGames; ++i) {
        int score = 0;
        int wave = 0;
        totalTicks += playWithPilot(seed, score, wave);
        totalScore += score;
        bestWave = std::max(bestWave, wave);
        seed = seed * 1664525u + 1013904223u;
    }
    const int averageSeconds = totalTicks / kGames / kTicksPerSecond;
    std::printf("      (pilot over %d games: avg %d s, avg score %d, best wave %d; idle player lasts %d s)\n",
                kGames, averageSeconds, totalScore / kGames, bestWave, idleTicks / kTicksPerSecond);
    check(totalTicks / kGames > idleTicks * 3, "pilot survives far longer than an idle player");
    check(totalScore / kGames >= 3000, "pilot averages at least 3000 points");
}

void runGame(game::Game& g, int ticks, game::Input input = {})
{
    for (int i = 0; i < ticks; ++i)
        g.update(input);
}

int countSound(const std::vector<game::Sound>& sounds, game::Sound sound)
{
    return int(std::count(sounds.begin(), sounds.end(), sound));
}

void testSoundEvents()
{
    game::World world = galleryWorld();
    game::DemoPilot pilot;
    std::vector<game::Sound> heard;
    for (int tick = 0; tick < 120 * kTicksPerSecond && world.aliensRemaining() > 0; ++tick) {
        world.update(pilot.update(world));
        heard.insert(heard.end(), world.sounds().begin(), world.sounds().end());
    }
    check(countSound(heard, game::Sound::AlienExplosion) == 44 &&
              countSound(heard, game::Sound::FlagshipExplosion) == 2,
          "every kill makes an explosion sound, flagships their own");
    check(countSound(heard, game::Sound::PlayerShot) >= 46, "every shot makes a sound");

    game::World attacking;
    heard.clear();
    for (int tick = 0; tick < 3 * 60 * kTicksPerSecond && !attacking.gameOver(); ++tick) {
        attacking.update({});
        heard.insert(heard.end(), attacking.sounds().begin(), attacking.sounds().end());
    }
    check(countSound(heard, game::Sound::Dive) > 0, "launching a dive makes the swoop sound");
    check(countSound(heard, game::Sound::PlayerExplosion) == 3, "each lost ship makes the player explosion sound");
}

void testAttractLoop()
{
    game::Game g(1000);
    bool sawScoreTable = false;
    bool sawDemo = false;
    bool returnedToTitle = false;
    for (int tick = 0; tick < 5 * 60 * kTicksPerSecond && !returnedToTitle; ++tick) {
        g.update({});
        sawScoreTable = sawScoreTable || g.mode() == game::Mode::ScoreTable;
        sawDemo = sawDemo || g.mode() == game::Mode::Demo;
        returnedToTitle = sawDemo && g.mode() == game::Mode::Title;
    }
    check(sawScoreTable && sawDemo && returnedToTitle, "attract mode cycles title, score table, demo, title");
    check(g.highScore() == 1000, "demo games never change the high score");

    game::Game g2(1000);
    std::vector<game::Sound> heard;
    bool humDuringDemo = false;
    bool humOnTitle = false;
    for (int tick = 0; tick < 40 * kTicksPerSecond; ++tick) {
        g2.update({});
        g2.takeSounds(heard);
        humDuringDemo = humDuringDemo || (g2.mode() == game::Mode::Demo && g2.humActive());
        humOnTitle = humOnTitle || (g2.mode() == game::Mode::Title && g2.humActive());
    }
    check(!heard.empty() && humDuringDemo && !humOnTitle, "the demo is heard; the title screen is quiet");
}

void testStartAndGameOver()
{
    game::Game g(10);
    runGame(g, 5); // keys count as held at launch; release them first
    game::Input start;
    start.start = true;
    g.update(start);
    check(g.mode() == game::Mode::Playing && g.world() && g.world()->score() == 0,
          "start from the title begins a game");

    // Idle until the aliens win.
    for (int i = 0; i < 10 * 60 * kTicksPerSecond && g.mode() == game::Mode::Playing; ++i)
        g.update({});
    check(g.mode() == game::Mode::GameOver, "losing the last ship shows game over");
    const int finalScore = g.world()->score();
    check(g.highScore() == std::max(10, finalScore), "a player's score can set the high score");

    runGame(g, 5 * kTicksPerSecond);
    check(g.mode() == game::Mode::Title, "game over returns to the title");
}

void testDeterministic()
{
    game::World a;
    game::World b;
    for (int tick = 0; tick < 3 * 60 * kTicksPerSecond; ++tick) {
        const game::Input input = sweepAndFire(tick);
        a.update(input);
        b.update(input);
    }
    check(a.score() == b.score() && a.wave() == b.wave() && a.aliensRemaining() == b.aliensRemaining() &&
              a.reserveShips() == b.reserveShips(),
          "same seed and inputs replay the same game");
    std::printf("      (sweep-and-fire for 3 minutes: score %d, wave %d)\n", a.score(), a.wave());
}

} // namespace

int main()
{
    testWaveCanBeCleared();
    testHoldingFireShootsOnce();
    testDivingPoints();
    testAliensAttack();
    testIdlePlayerLosesAllLives();
    testDeterministic();
    testPilotPlaysWell();
    testSoundEvents();
    testAttractLoop();
    testStartAndGameOver();
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
