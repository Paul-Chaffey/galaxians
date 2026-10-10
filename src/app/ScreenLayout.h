#pragma once

#include <array>

namespace app {

// An axis-aligned rectangle in window pixels.
struct Rect {
    float x = 0, y = 0, w = 0, h = 0;

    float right() const { return x + w; }
    float bottom() const { return y + h; }
    float centreX() const { return x + w * 0.5f; }
    float centreY() const { return y + h * 0.5f; }
    bool contains(float px, float py) const { return px >= x && px < right() && py >= y && py < bottom(); }
};

enum class TouchButton { Left, Right, Fire };
inline constexpr int kTouchButtonCount = 3;

// Where the playfield and the on-screen buttons go in the window.
struct ScreenLayout {
    float windowWidth = 0, windowHeight = 0;
    Rect game;                                   // the scaled-up 224x256 playfield
    std::array<Rect, kTouchButtonCount> buttons; // indexed by TouchButton
    // The window had no room beside the playfield, so the buttons cover its
    // lower corners and are drawn see-through.
    bool buttonsOverGame = false;

    const Rect& button(TouchButton b) const { return buttons[int(b)]; }
};

// Lays out a window of `width` x `height` pixels whose unobstructed part (away
// from notches and system bars) is `safe`. The playfield is scaled by the
// largest whole number that still leaves room for the buttons: below it in
// portrait, beside it in landscape.
ScreenLayout layoutScreen(float width, float height, Rect safe);

// The button under a point, or -1. Hit areas are a little larger than the
// drawn buttons; where two overlap, the nearer button wins.
int hitTest(const ScreenLayout& layout, float x, float y);

} // namespace app
