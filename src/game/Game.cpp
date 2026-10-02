#include "game/Game.h"

#include "game/Atlas.h"
#include "game/Hud.h"
#include "game/Text.h"

#include <algorithm>
#include <string>

namespace game {

namespace {

constexpr int kTicksPerSecond = 60;
constexpr int kTitleTicks = 7 * kTicksPerSecond;
constexpr int kScoreTableTicks = 7 * kTicksPerSecond;
constexpr int kDemoTicks = 75 * kTicksPerSecond; // the demo ends here even if the pilot survives
constexpr int kGameOverTicks = 4 * kTicksPerSecond;
constexpr int kGameOverInputDelay = kTicksPerSecond;

constexpr uint32_t kRed = gfx::rgba(0xE0, 0x20, 0x30);
constexpr uint32_t kYellow = gfx::rgba(0xFF, 0xD0, 0x00);
constexpr uint32_t kWhite = gfx::rgba(0xFF, 0xFF, 0xFF);
constexpr uint32_t kCyan = gfx::rgba(0x50, 0xD0, 0xFF);
constexpr uint32_t kGrey = gfx::rgba(0x90, 0x90, 0xA0);

constexpr float kStarScrollPerTick = 0.5f;

uint32_t nextSeed(uint32_t seed)
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

gfx::Sprite centred(const gfx::AtlasRect& rect, float x, float y)
{
    return gfx::Sprite::at(rect, x - rect.w * 0.5f, y - rect.h * 0.5f);
}

} // namespace

Game::Game(int highScore)
    : highScore_(highScore)
{
}

void Game::enter(Mode mode)
{
    mode_ = mode;
    modeTicks_ = 0;
}

void Game::startWorld(bool demo)
{
    WorldOptions options;
    options.seed = seed_;
    options.highScore = highScore_;
    options.recordsHighScore = !demo;
    seed_ = nextSeed(seed_);

    world_.emplace(options);
    pilot_ = DemoPilot{};
    if (demo) {
        enter(Mode::Demo);
    } else {
        lastScore_ = 0;
        suppressFire_ = true;
        enter(Mode::Playing);
    }
}

void Game::updateWorld(const Input& input)
{
    world_->update(input);
    sounds_.insert(sounds_.end(), world_->sounds().begin(), world_->sounds().end());
}

void Game::takeSounds(std::vector<Sound>& out)
{
    out.insert(out.end(), sounds_.begin(), sounds_.end());
    sounds_.clear();
}

bool Game::humActive() const
{
    return (mode_ == Mode::Playing || mode_ == Mode::Demo) && world_ && world_->aliensRemaining() > 0;
}

void Game::update(const Input& input)
{
    ++ticks_;
    ++modeTicks_;
    const bool startPressed = (input.start && !startHeld_) || (input.fire && !fireHeld_);
    startHeld_ = input.start;
    fireHeld_ = input.fire;

    switch (mode_) {
    case Mode::Title:
        if (startPressed)
            startWorld(false);
        else if (modeTicks_ >= kTitleTicks)
            enter(Mode::ScoreTable);
        break;

    case Mode::ScoreTable:
        if (startPressed)
            startWorld(false);
        else if (modeTicks_ >= kScoreTableTicks)
            startWorld(true);
        break;

    case Mode::Demo:
        if (startPressed) {
            startWorld(false);
            break;
        }
        updateWorld(pilot_.update(*world_));
        if (world_->gameOver())
            enter(Mode::GameOver);
        else if (modeTicks_ >= kDemoTicks)
            enter(Mode::Title);
        break;

    case Mode::Playing: {
        Input playerInput = input;
        if (suppressFire_) {
            if (input.fire)
                playerInput.fire = false;
            else
                suppressFire_ = false;
        }
        updateWorld(playerInput);
        lastScore_ = world_->score();
        highScore_ = std::max(highScore_, world_->highScore());
        if (world_->gameOver())
            enter(Mode::GameOver);
        break;
    }

    case Mode::GameOver:
        updateWorld({}); // the aliens carry on without you
        if (startPressed && modeTicks_ >= kGameOverInputDelay)
            startWorld(false);
        else if (modeTicks_ >= kGameOverTicks)
            enter(Mode::Title);
        break;
    }

    if (mode_ == Mode::Title || mode_ == Mode::ScoreTable)
        world_.reset();
}

void Game::render(std::vector<gfx::Sprite>& out, gfx::StarfieldState& stars, float alpha) const
{
    out.clear();
    if (world_) {
        world_->render(out, stars, alpha);
    } else {
        stars.scroll = (float(ticks_) - 1.0f + alpha) * kStarScrollPerTick;
        stars.blinkFrame = uint32_t(ticks_ / 15);
    }

    const bool blinkOn = (ticks_ / 30) % 2 == 0;
    switch (mode_) {
    case Mode::Title:
        renderTitle(out);
        renderAttractHud(out);
        break;
    case Mode::ScoreTable:
        renderScoreTable(out);
        renderAttractHud(out);
        break;
    case Mode::Demo:
        drawText(out, "DEMO", cell(23), cell(0), kCyan);
        if (blinkOn)
            drawCentredText(out, "PRESS FIRE TO START", 144.0f, kWhite);
        break;
    case Mode::Playing:
        break;
    case Mode::GameOver:
        drawCentredText(out, "GAME OVER", 136.0f, kRed);
        break;
    }
}

void Game::renderAttractHud(std::vector<gfx::Sprite>& out) const
{
    HudState hud;
    hud.score = lastScore_;
    hud.highScore = highScore_;
    hud.reserveShips = 0;
    hud.wave = 0;
    drawHud(out, hud);
}

void Game::renderTitle(std::vector<gfx::Sprite>& out) const
{
    drawCentredText(out, "GALAXIANS", 56.0f, kYellow, 2);
    drawCentredText(out, "WE ARE THE GALAXIANS", 96.0f, kWhite);
    drawCentredText(out, "MISSION: DESTROY ALIENS", 112.0f, kWhite);

    // A flapping line-up of the cast.
    const gfx::AtlasRect cast[][2] = {
        {atlas::kBlueAlienA, atlas::kBlueAlienB},
        {atlas::kPurpleAlienA, atlas::kPurpleAlienB},
        {atlas::kRedAlienA, atlas::kRedAlienB},
        {atlas::kFlagship, atlas::kFlagship},
        {atlas::kRedAlienA, atlas::kRedAlienB},
        {atlas::kPurpleAlienA, atlas::kPurpleAlienB},
        {atlas::kBlueAlienA, atlas::kBlueAlienB},
    };
    for (int i = 0; i < 7; ++i) {
        const bool wingsUp = ((ticks_ + uint64_t(i) * 4) / 16) % 2 == 0;
        out.push_back(centred(cast[i][wingsUp ? 0 : 1], 64.0f + float(i) * 16.0f, 144.0f));
    }

    if ((ticks_ / 30) % 2 == 0)
        drawCentredText(out, "PRESS FIRE TO START", 176.0f, kCyan);
    drawCentredText(out, "ARROWS MOVE - SPACE FIRES", 200.0f, kGrey);
    drawCentredText(out, "M MUTE - C CRT EFFECT", 212.0f, kGrey);
    drawCentredText(out, "F11 FULLSCREEN", 224.0f, kGrey);
}

void Game::renderScoreTable(std::vector<gfx::Sprite>& out) const
{
    drawCentredText(out, "- SCORE ADVANCE TABLE -", 48.0f, kRed);
    drawText(out, "CONVOY", cell(9), 72.0f, kCyan);
    drawText(out, "CHARGER", cell(17), 72.0f, kCyan);

    struct Row {
        AlienType type;
        gfx::AtlasRect sprite;
    };
    const Row rows[] = {
        {AlienType::Flagship, atlas::kFlagship},
        {AlienType::Red, atlas::kRedAlienA},
        {AlienType::Purple, atlas::kPurpleAlienA},
        {AlienType::Blue, atlas::kBlueAlienA},
    };

    // Rows appear one at a time, like the arcade.
    const int visible = std::min(4, modeTicks_ / 45);
    for (int i = 0; i < visible; ++i) {
        const float y = 96.0f + float(i) * 24.0f;
        out.push_back(centred(rows[i].sprite, 48.0f, y + 4.0f));

        const std::string convoy = std::to_string(formationPoints(rows[i].type));
        drawText(out, convoy, cell(15) - float(convoy.size()) * atlas::kGlyphSize, y, kWhite);

        const std::string charger = rows[i].type == AlienType::Flagship
                                        ? std::to_string(divingPoints(AlienType::Flagship, 0, 0)) + "-" +
                                              std::to_string(divingPoints(AlienType::Flagship, 2, 2))
                                        : std::to_string(divingPoints(rows[i].type, 0, 0));
        drawText(out, charger, cell(17), y, kWhite);
    }
}

} // namespace game
