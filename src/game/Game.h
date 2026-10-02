#pragma once

#include "game/DemoPilot.h"
#include "game/World.h"

#include <optional>
#include <vector>

namespace game {

// Attract loop:  Title -> ScoreTable -> Demo -> Title ...
// Start (from any attract screen) -> Playing -> GameOver -> Title.
enum class Mode { Title, ScoreTable, Demo, Playing, GameOver };

// Top-level flow around World: attract screens, the self-playing demo, the
// player's game and the session high score. Advanced in 60 Hz ticks.
class Game {
public:
    explicit Game(int highScore = 5000);

    void update(const Input& input);
    // `alpha` is the fraction of a tick elapsed since the last update; see World::render.
    void render(std::vector<gfx::Sprite>& sprites, gfx::StarfieldState& stars, float alpha = 1.0f) const;

    Mode mode() const { return mode_; }
    int highScore() const { return highScore_; }
    // The game being played or demonstrated, if any.
    const World* world() const { return world_ ? &*world_ : nullptr; }

    // Moves out the sound effects triggered since the last call.
    void takeSounds(std::vector<Sound>& out);
    // Whether the background hum should play: while aliens are on screen in a game or demo.
    bool humActive() const;

private:
    void enter(Mode mode);
    void updateWorld(const Input& input);
    void startWorld(bool demo);
    void renderTitle(std::vector<gfx::Sprite>& out) const;
    void renderScoreTable(std::vector<gfx::Sprite>& out) const;
    void renderAttractHud(std::vector<gfx::Sprite>& out) const;

    Mode mode_ = Mode::Title;
    int modeTicks_ = 0;
    uint64_t ticks_ = 0;
    std::optional<World> world_;
    DemoPilot pilot_;
    std::vector<Sound> sounds_;
    uint32_t seed_ = 0x6A1A7;

    int highScore_;
    int lastScore_ = 0;
    bool startHeld_ = true; // ignore keys already down at launch
    bool fireHeld_ = true;
    bool suppressFire_ = false; // swallow the press that started the game
};

} // namespace game
