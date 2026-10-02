#version 450

// Scales the low-resolution game image up to the window, optionally through a
// CRT look: slight screen curvature, glow, scanlines, an aperture-grille mask
// and a vignette. All maths is in linear light (both images are sRGB formats).
layout(set = 0, binding = 0) uniform sampler2D gameImage; // bilinear sampler

layout(push_constant) uniform PushConstants {
    uint crt;    // 0 = pixel-exact, 1 = CRT
    float scale; // window pixels per game pixel
} pc;

layout(location = 0) in vec2 inUV;
layout(location = 0) out vec4 outColor;

const float kCurvature = 0.035;
const float kGlow = 0.35;
const float kScanlineDepth = 0.45;
const float kMaskStrength = 0.12;
const float kVignette = 0.18;
const float kBrightness = 1.2; // makes up for light lost to scanlines and mask
const float kPi = 3.14159265;

vec3 fetch(vec2 texel, vec2 size)
{
    return texelFetch(gameImage, ivec2(clamp(texel, vec2(0.0), size - 1.0)), 0).rgb;
}

void main()
{
    vec2 size = vec2(textureSize(gameImage, 0));

    if (pc.crt == 0u) {
        outColor = vec4(fetch(inUV * size, size), 1.0);
        return;
    }

    // Barrel distortion: sample further out towards the edges, so the picture bulges.
    vec2 centred = inUV * 2.0 - 1.0;
    centred *= 1.0 + kCurvature * dot(centred, centred);
    vec2 uv = centred * 0.5 + 0.5;
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0)))) {
        outColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    vec2 texel = uv * size;
    vec3 color = fetch(texel, size);

    // Phosphor glow: a soft average of the neighbourhood, added on top. The
    // diagonals keep single-pixel stars from getting a "+"-shaped halo.
    vec2 step = 1.5 / size;
    vec3 glow = vec3(0.0);
    for (int y = -1; y <= 1; ++y)
        for (int x = -1; x <= 1; ++x)
            if (x != 0 || y != 0)
                glow += texture(gameImage, uv + vec2(x, y) * step).rgb * ((x == 0 || y == 0) ? 1.0 : 0.7);
    color += glow / 6.8 * kGlow;

    // Scanlines need a few window pixels per game row to resolve without moire.
    if (pc.scale >= 2.0) {
        float row = fract(texel.y);
        color *= 1.0 - kScanlineDepth + kScanlineDepth * sin(kPi * row);
    }

    // Aperture grille: alternate red, green and blue emphasis per window column.
    if (pc.scale >= 3.0) {
        vec3 mask = vec3(1.0 - kMaskStrength);
        mask[int(gl_FragCoord.x) % 3] = 1.0 + kMaskStrength;
        color *= mask;
    }

    color *= 1.0 - kVignette * dot(centred, centred);
    outColor = vec4(color * kBrightness, 1.0);
}
