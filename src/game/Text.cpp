#include "game/Text.h"

#include <array>
#include <cmath>

namespace game {

namespace {

// Maps an ASCII code to its index in atlas::kFontChars, or 0 (space).
constexpr std::array<uint8_t, 128> buildGlyphIndex()
{
    std::array<uint8_t, 128> index{};
    for (uint8_t i = 0; atlas::kFontChars[i] != '\0'; ++i)
        index[static_cast<uint8_t>(atlas::kFontChars[i])] = i;
    return index;
}

constexpr std::array<uint8_t, 128> kGlyphIndex = buildGlyphIndex();

} // namespace

void drawText(std::vector<gfx::Sprite>& out, std::string_view text, float x, float y, uint32_t tint, int scale)
{
    const float advance = atlas::kGlyphSize * float(scale);
    for (char ch : text) {
        if (ch >= 'a' && ch <= 'z')
            ch = static_cast<char>(ch - 'a' + 'A');
        const auto code = static_cast<unsigned char>(ch);
        const int glyph = code < kGlyphIndex.size() ? kGlyphIndex[code] : 0;

        if (glyph != 0) {
            gfx::AtlasRect rect{atlas::kFontX + float(glyph % atlas::kFontColumns) * atlas::kGlyphSize,
                                atlas::kFontY + float(glyph / atlas::kFontColumns) * atlas::kGlyphSize,
                                atlas::kGlyphSize, atlas::kGlyphSize};
            gfx::Sprite sprite = gfx::Sprite::at(rect, x, y);
            sprite.w = advance;
            sprite.h = advance;
            sprite.tint = tint;
            out.push_back(sprite);
        }
        x += advance;
    }
}

void drawCentredText(std::vector<gfx::Sprite>& out, std::string_view text, float y, uint32_t tint, int scale)
{
    constexpr float kScreenWidth = 224.0f;
    const float width = float(text.size()) * atlas::kGlyphSize * float(scale);
    drawText(out, text, std::floor((kScreenWidth - width) * 0.5f), y, tint, scale);
}

} // namespace game
