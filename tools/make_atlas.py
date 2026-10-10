#!/usr/bin/env python3
"""Builds assets/atlas.png and src/game/Atlas.h from the ASCII pixel art below,
and the Android launcher icons from the player's ship.

Run from anywhere: python tools/make_atlas.py
Each sprite gets its own 16x16 cell in a 128x128 atlas. '.' is transparent;
every other character is looked up in the sprite's palette. Below the sprites
the font is packed into 8x8 cells, one glyph per cell.
"""

import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ATLAS_SIZE = 128
CELL = 16

SHIP = [
    ".......W.......",
    ".......W.......",
    "......WWW......",
    "......WWW......",
    "...R..WWW..R...",
    "...R.WWWWW.R...",
    "...RWWWBWWWR...",
    "...WWWBBBWWW...",
    "..WWWWBBBWWWW..",
    ".WWWWWWWWWWWWW.",
    ".WWW.WWWWW.WWW.",
    ".WW..WW.WW..WW.",
]

ALIEN_WINGS_UP = [
    ".w...........w.",
    ".ww.........ww.",
    ".www...b...www.",
    "..www.bbb.www..",
    "...wwbbbbbww...",
    "....bebbbeb....",
    "....bbbbbbb....",
    ".....bbbbb.....",
    "....b.bbb.b....",
    "...b..b.b..b...",
]

ALIEN_WINGS_DOWN = [
    "...............",
    "...............",
    ".......b.......",
    "...ww.bbb.ww...",
    ".wwwwbbbbbwwww.",
    ".ww.bebbbeb.ww.",
    ".w..bbbbbbb..w.",
    ".....bbbbb.....",
    "....b.bbb.b....",
    "...b..b.b..b...",
]

FLAGSHIP = [
    ".......Y.......",
    "......YYY......",
    "..r...YYY...r..",
    "..rr.YYYYY.rr..",
    "..rrrYRYRYrrr..",
    ".rrrrYYYYYrrrr.",
    ".rr..YYYYY..rr.",
    ".r....YYY....r.",
    "......Y.Y......",
    ".....Y...Y.....",
]

PLAYER_SHOT = ["Y", "W", "W", "W"]
ALIEN_SHOT = ["W", "W", "W"]
PIXEL = ["W"]

EXPLOSION_1 = [
    "...............",
    "...............",
    "...............",
    ".....R...R.....",
    "......Y.Y......",
    ".......W.......",
    "......Y.Y......",
    ".....R...R.....",
    "...............",
    "...............",
    "...............",
]

EXPLOSION_2 = [
    "...............",
    "...R.......R...",
    "....R..Y..R....",
    ".....Y.W.Y.....",
    "..R..YWWWY..R..",
    "...YWWWWWWWY...",
    "..R..YWWWY..R..",
    ".....Y.W.Y.....",
    "....R..Y..R....",
    "...R.......R...",
    "...............",
]

EXPLOSION_3 = [
    ".R.....R.....R.",
    "..R...Y.Y...R..",
    "...Y..R.R..Y...",
    "R...Y.Y.Y.Y...R",
    "..Y.R.....R.Y..",
    ".YR..W...W..RY.",
    "..Y.R.....R.Y..",
    "R...Y.Y.Y.Y...R",
    "...Y..R.R..Y...",
    "..R...Y.Y...R..",
    ".R.....R.....R.",
]

EXPLOSION_4 = [
    "R.............R",
    "...............",
    "..R....R....R..",
    "...............",
    "R....R...R....R",
    "...............",
    "R....R...R....R",
    "...............",
    "..R....R....R..",
    "...............",
    "R.............R",
]

PLAYER_EXPLOSION_1 = [
    "...............",
    "......W.W......",
    "...R...W...R...",
    "....R.WWW.R....",
    ".....WWWWW.....",
    "..W.WWWBWWW.W..",
    ".....WWWWW.....",
    "....R.WWW.R....",
    "...R...W...R...",
    "......W.W......",
    "...............",
]

PLAYER_EXPLOSION_2 = [
    "W......W......W",
    "..R....Y....R..",
    "....Y.R.R.Y....",
    ".W...W.Y.W...W.",
    "...R.Y...Y.R...",
    "Y.R.W.....W.R.Y",
    "...R.Y...Y.R...",
    ".W...W.Y.W...W.",
    "....Y.R.R.Y....",
    "..R....Y....R..",
    "W......W......W",
]

PLAYER_EXPLOSION_3 = [
    "R......R......R",
    "...............",
    "..Y.........Y..",
    "...............",
    "R.....Y.Y.....R",
    "...............",
    "R.....Y.Y.....R",
    "...............",
    "..Y.........Y..",
    "...............",
    "R......R......R",
]

FLAG = [
    "Y......",
    "YRRRRRR",
    "YRRRRR.",
    "YRRRR..",
    "YRRRRR.",
    "YRRRRRR",
    "Y......",
    "Y......",
]

# On-screen touch controls (Android). The button is tinted when drawn; the
# arrow points left and is mirrored for the right button.
BUTTON = [
    ".....WWWWW.....",
    "...WWGGGGGWW...",
    "..WGGGGGGGGGW..",
    ".WGGGGGGGGGGGW.",
    ".WGGGGGGGGGGGW.",
    "WGGGGGGGGGGGGGW",
    "WGGGGGGGGGGGGGW",
    "WGGGGGGGGGGGGGW",
    "WGGGGGGGGGGGGGW",
    "WGGGGGGGGGGGGGW",
    ".WGGGGGGGGGGGW.",
    ".WGGGGGGGGGGGW.",
    "..WGGGGGGGGGW..",
    "...WWGGGGGWW...",
    ".....WWWWW.....",
]

ARROW = [
    "....W....",
    "...WW....",
    "..WWWWWWW",
    ".WWWWWWWW",
    "WWWWWWWWW",
    ".WWWWWWWW",
    "..WWWWWWW",
    "...WW....",
    "....W....",
]

FIRE_ICON = [
    "....Y....",
    ".Y..Y..Y.",
    "..Y.Y.Y..",
    "...YYY...",
    "YYYYWYYYY",
    "...YYY...",
    "..Y.Y.Y..",
    ".Y..Y..Y.",
    "....Y....",
]

# 5x7 glyphs, drawn in white so text can be tinted any colour.
FONT_CHARS = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ-.!:"
FONT_CELL = 8
FONT_Y = 64  # sprites may use the 16x16 cells above this
FONT_GLYPHS = """
.XXX. ..X.. .XXX. XXXXX ...X. XXXXX ..XX. XXXXX .XXX. .XXX.
X...X .XX.. X...X ...X. ..XX. X.... .X... ....X X...X X...X
X..XX ..X.. ....X ..X.. .X.X. XXXX. X.... ...X. X...X X...X
X.X.X ..X.. ...X. ...X. X..X. ....X XXXX. ..X.. .XXX. .XXXX
XX..X ..X.. ..X.. ....X XXXXX ....X X...X .X... X...X ....X
X...X ..X.. .X... X...X ...X. X...X X...X .X... X...X ...X.
.XXX. .XXX. XXXXX .XXX. ...X. .XXX. .XXX. .X... .XXX. .XX..

.XXX. XXXX. .XXX. XXX.. XXXXX XXXXX .XXX. X...X .XXX. ..XXX
X...X X...X X...X X..X. X.... X.... X...X X...X ..X.. ...X.
X...X X...X X.... X...X X.... X.... X.... X...X ..X.. ...X.
XXXXX XXXX. X.... X...X XXXX. XXXX. X.XXX XXXXX ..X.. ...X.
X...X X...X X.... X...X X.... X.... X...X X...X ..X.. ...X.
X...X X...X X...X X..X. X.... X.... X...X X...X ..X.. X..X.
X...X XXXX. .XXX. XXX.. XXXXX X.... .XXXX X...X .XXX. .XX..

X...X X.... X...X X...X .XXX. XXXX. .XXX. XXXX. .XXXX XXXXX
X..X. X.... XX.XX X...X X...X X...X X...X X...X X.... ..X..
X.X.. X.... X.X.X XX..X X...X X...X X...X X...X X.... ..X..
XX... X.... X.X.X X.X.X X...X XXXX. X...X XXXX. .XXX. ..X..
X.X.. X.... X...X X..XX X...X X.... X.X.X X.X.. ....X ..X..
X..X. X.... X...X X...X X...X X.... X..X. X..X. ....X ..X..
X...X XXXXX X...X X...X .XXX. X.... .XX.X X...X XXXX. ..X..

X...X X...X X...X X...X X...X XXXXX ..... ..... ..X.. .....
X...X X...X X...X X...X X...X ....X ..... ..... ..X.. .XX..
X...X X...X X...X .X.X. .X.X. ...X. ..... ..... ..X.. .XX..
X...X X...X X.X.X ..X.. ..X.. ..X.. XXXXX ..... ..X.. .....
X...X X...X X.X.X .X.X. ..X.. .X... ..... ..... ..X.. .XX..
X...X .X.X. X.X.X X...X ..X.. X.... ..... .XX.. ..... .XX..
.XXX. ..X.. .X.X. X...X ..X.. XXXXX ..... .XX.. ..X.. .....
"""


def parse_font():
    """Returns the glyph rows for FONT_CHARS[1:] (space is left blank)."""
    glyphs = []
    for block in FONT_GLYPHS.strip().split("\n\n"):
        rows = [line.split() for line in block.splitlines()]
        for column in range(len(rows[0])):
            glyphs.append([row[column] for row in rows])
    assert len(glyphs) == len(FONT_CHARS) - 1, f"{len(glyphs)} glyphs for {len(FONT_CHARS) - 1} characters"
    return glyphs

WHITE = (0xF0, 0xF0, 0xFF)
RED = (0xE0, 0x20, 0x30)
BLUE = (0x30, 0x60, 0xFF)
YELLOW = (0xFF, 0xD0, 0x00)

SHIP_PALETTE = {"W": WHITE, "R": RED, "B": BLUE}
FLAGSHIP_PALETTE = {"Y": YELLOW, "R": (0xFF, 0x60, 0x00), "r": RED}
SHOT_PALETTE = {"W": (0xFF, 0xFF, 0xFF), "Y": YELLOW}
FLAG_PALETTE = {"Y": YELLOW, "R": RED}
EXPLOSION_PALETTE = {"W": (0xFF, 0xFF, 0xFF), "Y": YELLOW, "R": RED, "B": BLUE}
BUTTON_PALETTE = {"W": (0xFF, 0xFF, 0xFF), "G": (0x50, 0x50, 0x50)}
ICON_PALETTE = {"W": (0xFF, 0xFF, 0xFF), "Y": YELLOW}


def alien_palette(body, wings):
    return {"b": body, "w": wings, "e": YELLOW}


BLUE_ALIEN = alien_palette(BLUE, (0x50, 0xD0, 0xFF))
PURPLE_ALIEN = alien_palette((0xA0, 0x40, 0xE0), (0xFF, 0x70, 0xFF))
RED_ALIEN = alien_palette(RED, (0xFF, 0x90, 0x30))

# (C++ name, art, palette) in atlas order. Sprites are checked for left/right
# symmetry (a cheap typo catch) unless listed in ASYMMETRIC.
SPRITES = [
    ("kShip", SHIP, SHIP_PALETTE),
    ("kFlagship", FLAGSHIP, FLAGSHIP_PALETTE),
    ("kRedAlienA", ALIEN_WINGS_UP, RED_ALIEN),
    ("kRedAlienB", ALIEN_WINGS_DOWN, RED_ALIEN),
    ("kPurpleAlienA", ALIEN_WINGS_UP, PURPLE_ALIEN),
    ("kPurpleAlienB", ALIEN_WINGS_DOWN, PURPLE_ALIEN),
    ("kBlueAlienA", ALIEN_WINGS_UP, BLUE_ALIEN),
    ("kBlueAlienB", ALIEN_WINGS_DOWN, BLUE_ALIEN),
    ("kPlayerShot", PLAYER_SHOT, SHOT_PALETTE),
    ("kAlienShot", ALIEN_SHOT, SHOT_PALETTE),
    ("kPixel", PIXEL, SHOT_PALETTE),
    ("kFlag", FLAG, FLAG_PALETTE),
    ("kExplosion1", EXPLOSION_1, EXPLOSION_PALETTE),
    ("kExplosion2", EXPLOSION_2, EXPLOSION_PALETTE),
    ("kExplosion3", EXPLOSION_3, EXPLOSION_PALETTE),
    ("kExplosion4", EXPLOSION_4, EXPLOSION_PALETTE),
    ("kPlayerExplosion1", PLAYER_EXPLOSION_1, EXPLOSION_PALETTE),
    ("kPlayerExplosion2", PLAYER_EXPLOSION_2, EXPLOSION_PALETTE),
    ("kPlayerExplosion3", PLAYER_EXPLOSION_3, EXPLOSION_PALETTE),
    ("kButton", BUTTON, BUTTON_PALETTE),
    ("kArrow", ARROW, ICON_PALETTE),
    ("kFireIcon", FIRE_ICON, ICON_PALETTE),
]
ASYMMETRIC = {"kFlag", "kArrow"}


def validate(name, art, palette):
    width = len(art[0])
    assert width <= CELL and len(art) <= CELL, f"{name} is larger than a {CELL}x{CELL} cell"
    for row in art:
        assert len(row) == width, f"{name}: ragged row {row!r}"
        if name not in ASYMMETRIC:
            assert row == row[::-1], f"{name}: row {row!r} is not symmetric"
        for ch in row:
            assert ch == "." or ch in palette, f"{name}: no colour for {ch!r}"


def write_png(path, width, height, rgba_rows):
    raw = b"".join(b"\x00" + bytes(row) for row in rgba_rows)

    def chunk(tag, data):
        body = tag + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body))

    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 9))
    png += chunk(b"IEND", b"")
    path.write_bytes(png)


# Android launcher icon sizes (pixels) by screen density.
ICON_SIZES = {"mdpi": 48, "hdpi": 72, "xhdpi": 96, "xxhdpi": 144, "xxxhdpi": 192}
ICON_BACKGROUND = (0x00, 0x00, 0x18)


def write_icons():
    """The ship, scaled up by whole pixels on a night-sky square."""
    art_w, art_h = len(SHIP[0]), len(SHIP)
    for density, size in ICON_SIZES.items():
        scale = max(1, size * 2 // 3 // art_w)
        ox = (size - art_w * scale) // 2
        oy = (size - art_h * scale) // 2
        rows = []
        for y in range(size):
            row = []
            for x in range(size):
                ax, ay = (x - ox) // scale, (y - oy) // scale
                ch = SHIP[ay][ax] if 0 <= ax < art_w and 0 <= ay < art_h and x >= ox and y >= oy else "."
                row += list(SHIP_PALETTE[ch] if ch != "." else ICON_BACKGROUND) + [255]
            rows.append(row)
        out = ROOT / "android" / "app" / "src" / "main" / "res" / f"mipmap-{density}"
        out.mkdir(parents=True, exist_ok=True)
        write_png(out / "ic_launcher.png", size, size, rows)


def main():
    write_icons()
    pixels = [[0] * (ATLAS_SIZE * 4) for _ in range(ATLAS_SIZE)]
    cells_per_row = ATLAS_SIZE // CELL
    rects = []

    sprite_rows = -(-len(SPRITES) // cells_per_row)
    assert sprite_rows * CELL <= FONT_Y, "sprites overflow into the font area; raise FONT_Y"

    for index, (name, art, palette) in enumerate(SPRITES):
        validate(name, art, palette)
        ox = (index % cells_per_row) * CELL
        oy = (index // cells_per_row) * CELL
        for y, row in enumerate(art):
            for x, ch in enumerate(row):
                if ch == ".":
                    continue
                r, g, b = palette[ch]
                i = (ox + x) * 4
                pixels[oy + y][i : i + 4] = [r, g, b, 255]
        rects.append((name, ox, oy, len(art[0]), len(art)))

    font_columns = ATLAS_SIZE // FONT_CELL
    for index, glyph in enumerate(parse_font(), start=1):
        ox = (index % font_columns) * FONT_CELL + 1  # 1px left margin in the cell
        oy = FONT_Y + (index // font_columns) * FONT_CELL
        for y, row in enumerate(glyph):
            for x, ch in enumerate(row):
                if ch == "X":
                    i = (ox + x) * 4
                    pixels[oy + y][i : i + 4] = [255, 255, 255, 255]

    write_png(ROOT / "assets" / "atlas.png", ATLAS_SIZE, ATLAS_SIZE, pixels)

    lines = [
        "// Generated by tools/make_atlas.py. Do not edit; change the script and rerun it.",
        "#pragma once",
        "",
        '#include "gfx/Sprite.h"',
        "",
        "namespace atlas {",
        "",
    ]
    for name, x, y, w, h in rects:
        lines.append(f"inline constexpr gfx::AtlasRect {name}{{{x}, {y}, {w}, {h}}};")
    lines += [
        "",
        f"// Font: glyph i of kFontChars is the {FONT_CELL}x{FONT_CELL} cell at",
        "// (kFontX + (i % kFontColumns) * kGlyphSize, kFontY + (i / kFontColumns) * kGlyphSize).",
        f'inline constexpr char kFontChars[] = "{FONT_CHARS}";',
        "inline constexpr float kFontX = 0;",
        f"inline constexpr float kFontY = {FONT_Y};",
        f"inline constexpr int kFontColumns = {font_columns};",
        f"inline constexpr float kGlyphSize = {FONT_CELL};",
        "",
        "} // namespace atlas",
        "",
    ]
    (ROOT / "src" / "game" / "Atlas.h").write_text("\n".join(lines))


if __name__ == "__main__":
    main()
