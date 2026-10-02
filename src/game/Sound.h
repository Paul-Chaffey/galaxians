#pragma once

namespace game {

// One-shot sound effects the game asks for. The background hum is not an
// event; it follows Game::humActive().
enum class Sound {
    PlayerShot,
    AlienExplosion,
    FlagshipExplosion,
    PlayerExplosion,
    Dive,
    Count
};

} // namespace game
