#version 450

// Procedural scrolling starfield, evaluated per virtual-screen pixel. A pixel
// is a star if a hash of its scrolled position falls under a threshold, so the
// field needs no textures or buffers and never repeats in practice.
layout(push_constant) uniform PushConstants {
    uint scroll;     // whole pixels scrolled so far; stars move down as it grows
    uint blinkFrame; // advances to make one of the four star groups blink out
} pc;

layout(location = 0) out vec4 outColor;

// Integer hash (lowbias32 by Chris Wellons).
uint hash(uint x)
{
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

// Per-channel intensity levels, as on the original hardware's 2-bit-per-channel stars.
const float kLevels[4] = float[](0.0, 0.59, 0.87, 1.0);

// Exact sRGB-to-linear transfer function.
vec3 srgbToLinear(vec3 c)
{
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), greaterThan(c, vec3(0.04045)));
}

void main()
{
    uvec2 p = uvec2(gl_FragCoord.xy);
    uint worldY = p.y - pc.scroll; // wraps harmlessly
    uint h = hash(p.x * 0x9e3779b9u ^ hash(worldY));

    // Roughly 1 pixel in 256 is a star.
    if ((h & 0xFFu) != 0u)
        discard;

    uint group = (h >> 8) & 3u;
    if (group == (pc.blinkFrame & 3u))
        discard;

    uvec3 c = uvec3(h >> 10, h >> 12, h >> 14) & 3u;
    if (all(equal(c, uvec3(0u))))
        c = uvec3(3u); // no black stars

    // Levels are sRGB; the attachment is sRGB, so convert to linear first.
    vec3 srgb = vec3(kLevels[c.r], kLevels[c.g], kLevels[c.b]);
    outColor = vec4(srgbToLinear(srgb), 1.0);
}
