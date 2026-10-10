// Checks the Android screen layout: where the playfield and on-screen buttons go.
#include "app/ScreenLayout.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

using app::Rect;
using app::ScreenLayout;
using app::TouchButton;

int failures = 0;

void check(bool ok, const char* what)
{
    std::printf("%s: %s\n", ok ? "PASS" : "FAIL", what);
    if (!ok)
        ++failures;
}

bool overlaps(const Rect& a, const Rect& b)
{
    return a.x < b.right() && b.x < a.right() && a.y < b.bottom() && b.y < a.bottom();
}

bool inside(const Rect& inner, const Rect& outer)
{
    return inner.x >= outer.x && inner.y >= outer.y && inner.right() <= outer.right() &&
           inner.bottom() <= outer.bottom();
}

bool wholeScale(const Rect& game, float scale)
{
    return game.w == 224 * scale && game.h == 256 * scale;
}

// Every button is inside the safe area, clear of the playfield and of each other.
bool buttonsClear(const ScreenLayout& layout, const Rect& safe)
{
    for (int i = 0; i < app::kTouchButtonCount; ++i) {
        const Rect& b = layout.buttons[i];
        if (b.w <= 0 || !inside(b, safe) || overlaps(b, layout.game))
            return false;
        for (int j = i + 1; j < app::kTouchButtonCount; ++j)
            if (overlaps(b, layout.buttons[j]))
                return false;
    }
    return true;
}

int hitCentre(const ScreenLayout& layout, TouchButton button)
{
    const Rect& b = layout.button(button);
    return app::hitTest(layout, b.centreX(), b.centreY());
}

} // namespace

int main()
{
    // A 1080x2400 phone held upright, with a status bar and a gesture bar.
    const Rect portraitSafe{0, 110, 1080, 2400 - 110 - 60};
    const ScreenLayout portrait = app::layoutScreen(1080, 2400, portraitSafe);
    check(wholeScale(portrait.game, 4), "portrait phone draws the playfield at 4x");
    check(portrait.game.y == portraitSafe.y, "portrait playfield sits at the top of the safe area");
    check(inside(portrait.game, portraitSafe), "portrait playfield is inside the safe area");
    check(!portrait.buttonsOverGame && buttonsClear(portrait, portraitSafe),
          "portrait buttons are below the playfield, in the safe area, apart");
    check(portrait.button(TouchButton::Left).right() < portrait.button(TouchButton::Right).x &&
              portrait.button(TouchButton::Fire).x > portraitSafe.centreX(),
          "portrait: Left then Right on the left, Fire on the right");

    // The same phone on its side, with a notch on the left and gesture zones at
    // the top and bottom edges. The playfield may cover those; the buttons may not.
    const Rect landscapeSafe{110, 40, 2400 - 110, 1080 - 40 - 60};
    const ScreenLayout landscape = app::layoutScreen(2400, 1080, landscapeSafe);
    check(wholeScale(landscape.game, 4), "landscape phone draws the playfield at 4x");
    check(inside(landscape.game, Rect{0, 0, 2400, 1080}), "landscape playfield is inside the window");
    check(!landscape.buttonsOverGame && buttonsClear(landscape, landscapeSafe),
          "landscape buttons are in the side bars, in the safe area, apart");
    check(landscape.button(TouchButton::Right).right() <= landscape.game.x &&
              landscape.button(TouchButton::Fire).x >= landscape.game.right(),
          "landscape: movement in the left bar, Fire in the right");

    // A 4:3 tablet on its side has narrow bars at full size, so the playfield shrinks to make room.
    const Rect tabletSafe{0, 0, 2048, 1536};
    const ScreenLayout tablet = app::layoutScreen(2048, 1536, tabletSafe);
    check(!tablet.buttonsOverGame && buttonsClear(tablet, tabletSafe) &&
              std::floor(tablet.game.w / 224) == tablet.game.w / 224,
          "landscape tablet shrinks the playfield to a smaller whole scale for the buttons");

    // A square window has no room beside the playfield at any scale.
    const Rect squareSafe{0, 0, 300, 300};
    const ScreenLayout square = app::layoutScreen(300, 300, squareSafe);
    check(square.buttonsOverGame, "square window puts the buttons over the playfield");
    check(inside(square.game, squareSafe), "square window still fits the playfield");

    check(hitCentre(portrait, TouchButton::Left) == int(TouchButton::Left) &&
              hitCentre(portrait, TouchButton::Right) == int(TouchButton::Right) &&
              hitCentre(portrait, TouchButton::Fire) == int(TouchButton::Fire),
          "touching a button's centre presses it");
    const Rect& left = portrait.button(TouchButton::Left);
    const Rect& right = portrait.button(TouchButton::Right);
    check(app::hitTest(portrait, left.right() + 1, left.centreY()) == int(TouchButton::Left),
          "just past Left's edge, towards Right, still presses Left");
    check(app::hitTest(portrait, right.x - 1, right.centreY()) == int(TouchButton::Right),
          "just before Right's edge still presses Right");
    check(app::hitTest(portrait, portrait.game.centreX(), portrait.game.centreY()) == -1,
          "touching the playfield presses nothing");

    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
