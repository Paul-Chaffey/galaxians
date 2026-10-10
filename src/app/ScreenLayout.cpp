#include "app/ScreenLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace app {

namespace {

constexpr float kGameWidth = 224;
constexpr float kGameHeight = 256;

// Button sizes, as fractions of the shorter side of the safe area and then of
// the movement button's diameter.
constexpr float kButtonFraction = 0.2f;
constexpr float kFireSize = 1.2f;
constexpr float kMargin = 0.25f;
constexpr float kGap = 0.15f;
// Buttons may shrink to this fraction of their full size to make room for a
// bigger playfield; any smaller and they are hard to hit.
constexpr float kMinButtonFraction = 0.6f;
// Spare space, in button diameters, each orientation needs: a row of buttons
// below the playfield, or Left and Right side by side in a side bar.
constexpr float kPortraitBand = kFireSize + 2 * kMargin;
constexpr float kLandscapeBar = 2 + kGap + 2 * kMargin;
// Hit areas extend this far, in button diameters, beyond the drawn button.
constexpr float kHitSlop = 0.15f;

// Largest whole-number scale at which the playfield fits, or a fractional one
// when even 1x does not fit.
float fitScale(float width, float height)
{
    const float scale = std::min(width / kGameWidth, height / kGameHeight);
    return scale >= 1.0f ? std::floor(scale) : scale;
}

Rect gameRect(float scale, float x, float y)
{
    return {std::floor(x), std::floor(y), kGameWidth * scale, kGameHeight * scale};
}

Rect centredGame(Rect area, float scale)
{
    return gameRect(scale, area.x + (area.w - kGameWidth * scale) * 0.5f,
                    area.y + (area.h - kGameHeight * scale) * 0.5f);
}

// Left and Right at the left edge, Fire at the right, all centred on `centreY`.
void placeButtons(ScreenLayout& layout, Rect safe, float size, float centreY)
{
    const float margin = size * kMargin;
    const float fire = size * kFireSize;
    Rect& left = layout.buttons[int(TouchButton::Left)];
    Rect& right = layout.buttons[int(TouchButton::Right)];
    left = {safe.x + margin, centreY - size * 0.5f, size, size};
    right = {left.right() + size * kGap, left.y, size, size};
    layout.buttons[int(TouchButton::Fire)] = {safe.right() - margin - fire, centreY - fire * 0.5f, fire, fire};
}

} // namespace

ScreenLayout layoutScreen(float width, float height, Rect safe)
{
    ScreenLayout layout;
    layout.windowWidth = width;
    layout.windowHeight = height;

    // The safe area keeps clear of system gesture zones as well as notches. The
    // buttons must stay inside it, but the playfield only needs to avoid what
    // can cover it: a notch at the top in portrait, nothing in landscape, where
    // notches sit in the side bars.
    const bool portrait = safe.h >= safe.w;
    const float fullSize = std::min(safe.w, safe.h) * kButtonFraction;
    const float fullScale = portrait ? fitScale(width, height - safe.y) : fitScale(safe.w, height);

    // Try whole-number scales from the largest down until the buttons fit beside the playfield.
    for (float scale = fullScale; scale >= 1.0f; scale -= 1.0f) {
        if (portrait) {
            const float space = safe.h - kGameHeight * scale;
            const float size = std::min(fullSize, space / kPortraitBand);
            if (size < fullSize * kMinButtonFraction)
                continue;
            // Playfield at the top; buttons low in the space below it, where thumbs rest.
            layout.game = gameRect(scale, (width - kGameWidth * scale) * 0.5f, safe.y);
            const float lowest = safe.bottom() - size * (2 * kMargin + kFireSize * 0.5f);
            const float middle = layout.game.bottom() + space * 0.5f;
            placeButtons(layout, safe, size, std::max(lowest, middle));
            return layout;
        }
        const float space = (safe.w - kGameWidth * scale) * 0.5f;
        const float size = std::min(fullSize, space / kLandscapeBar);
        if (size < fullSize * kMinButtonFraction)
            continue;
        // Playfield centred between the side bars; buttons in their lower part.
        layout.game = gameRect(scale, safe.x + (safe.w - kGameWidth * scale) * 0.5f,
                               (height - kGameHeight * scale) * 0.5f);
        placeButtons(layout, safe, size, safe.bottom() - size * (2 * kMargin + kFireSize * 0.5f));
        return layout;
    }

    // No room beside the playfield: buttons over its lower corners.
    layout.game = centredGame(safe, fitScale(safe.w, safe.h));
    placeButtons(layout, safe, fullSize, safe.bottom() - fullSize * (kMargin + kFireSize * 0.5f));
    layout.buttonsOverGame = true;
    return layout;
}

int hitTest(const ScreenLayout& layout, float x, float y)
{
    int best = -1;
    float bestDistance = std::numeric_limits<float>::max();
    for (int i = 0; i < kTouchButtonCount; ++i) {
        const Rect& b = layout.buttons[i];
        const float slop = b.w * kHitSlop;
        const Rect area{b.x - slop, b.y - slop, b.w + 2 * slop, b.h + 2 * slop};
        if (b.w <= 0 || !area.contains(x, y))
            continue;
        const float dx = x - b.centreX();
        const float dy = y - b.centreY();
        const float distance = dx * dx + dy * dy;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

} // namespace app
