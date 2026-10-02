#pragma once

#include <cstdint>

namespace gfx {

// A rectangle in the sprite atlas, in atlas pixels.
struct AtlasRect {
    float x, y, w, h;
};

// Packs a colour so that it reads back as R, G, B, A from memory (VK_FORMAT_R8G8B8A8_UNORM).
constexpr uint32_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255)
{
    return uint32_t(r) | uint32_t(g) << 8 | uint32_t(b) << 16 | uint32_t(a) << 24;
}

// One sprite instance. The layout matches the per-instance inputs in shaders/sprite.vert.
struct Sprite {
    float x = 0, y = 0, w = 0, h = 0;   // top-left and size, in virtual-screen pixels
    float u = 0, v = 0, uw = 0, vh = 0; // source rect in atlas pixels; negative uw/vh mirrors
    float rotation = 0;                 // radians, clockwise about the sprite centre
    uint32_t tint = rgba(255, 255, 255);

    static Sprite at(const AtlasRect& r, float x, float y)
    {
        return {x, y, r.w, r.h, r.x, r.y, r.w, r.h};
    }
};

static_assert(sizeof(Sprite) == 40, "Sprite must match the vertex input layout");

} // namespace gfx
