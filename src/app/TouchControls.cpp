#include "app/TouchControls.h"

#include "game/Atlas.h"

#include <algorithm>
#include <cstdlib>

namespace app {

namespace {

constexpr uint8_t kIdleAlpha = 110;
constexpr uint8_t kHeldAlpha = 230;
// Drawn over the playfield, the buttons must not hide the ship.
constexpr uint8_t kOverGameAlpha = 70;

// The icon fills this fraction of its button.
constexpr float kIconFraction = 0.45f;

gfx::Sprite stretched(const gfx::AtlasRect& r, const Rect& to, uint32_t tint)
{
    gfx::Sprite s = gfx::Sprite::at(r, to.x, to.y);
    s.w = to.w;
    s.h = to.h;
    s.tint = tint;
    return s;
}

Rect iconRect(const Rect& button)
{
    const float size = button.w * kIconFraction;
    return {button.centreX() - size * 0.5f, button.centreY() - size * 0.5f, size, size};
}

} // namespace

void TouchControls::handleEvent(const SDL_Event& event)
{
    switch (event.type) {
    case SDL_EVENT_FINGER_DOWN:
        visible_ = true;
        press(event.tfinger.fingerID, event.tfinger.x, event.tfinger.y);
        break;
    case SDL_EVENT_FINGER_MOTION:
        press(event.tfinger.fingerID, event.tfinger.x, event.tfinger.y);
        break;
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        release(event.tfinger.fingerID);
        break;
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        visible_ = false;
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        // Sticks rest slightly off centre; only a deliberate push counts.
        if (std::abs(event.gaxis.value) > 16000)
            visible_ = false;
        break;
    default:
        break;
    }
}

void TouchControls::press(SDL_FingerID id, float x, float y)
{
    // Finger positions arrive normalised to the window.
    const int button = hitTest(layout_, x * layout_.windowWidth, y * layout_.windowHeight);
    if (button >= 0)
        tapped_[button] = true;
    auto it = std::find_if(fingers_.begin(), fingers_.end(), [&](const Finger& f) { return f.id == id; });
    if (it != fingers_.end())
        it->button = button;
    else
        fingers_.push_back({id, button});
}

void TouchControls::release(SDL_FingerID id)
{
    std::erase_if(fingers_, [&](const Finger& f) { return f.id == id; });
}

bool TouchControls::held(TouchButton button) const
{
    return std::any_of(fingers_.begin(), fingers_.end(), [&](const Finger& f) { return f.button == int(button); });
}

void TouchControls::read(game::Input& input)
{
    auto pressed = [&](TouchButton b) { return held(b) || tapped_[int(b)]; };
    input.left |= pressed(TouchButton::Left);
    input.right |= pressed(TouchButton::Right);
    // Fire also starts a game from the attract mode.
    input.fire |= pressed(TouchButton::Fire);
    tapped_ = {};
}

void TouchControls::draw(std::vector<gfx::Sprite>& out) const
{
    if (!visible_)
        return;

    for (int i = 0; i < kTouchButtonCount; ++i) {
        const auto button = TouchButton(i);
        const Rect& rect = layout_.button(button);
        uint8_t alpha = held(button) ? kHeldAlpha : kIdleAlpha;
        if (layout_.buttonsOverGame)
            alpha = std::min(alpha, kOverGameAlpha);

        const uint32_t ring = button == TouchButton::Fire ? gfx::rgba(255, 90, 70, alpha) : gfx::rgba(120, 170, 255, alpha);
        out.push_back(stretched(atlas::kButton, rect, ring));

        const uint32_t icon = gfx::rgba(255, 255, 255, alpha);
        if (button == TouchButton::Fire) {
            out.push_back(stretched(atlas::kFireIcon, iconRect(rect), icon));
            continue;
        }
        gfx::Sprite arrow = stretched(atlas::kArrow, iconRect(rect), icon);
        if (button == TouchButton::Right) {
            // The atlas arrow points left; read it right to left to mirror it.
            arrow.u += arrow.uw;
            arrow.uw = -arrow.uw;
        }
        out.push_back(arrow);
    }
}

} // namespace app
