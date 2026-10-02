#version 450

// Per-instance data; see gfx::Sprite.
layout(location = 0) in vec4 inRect;  // x, y, w, h in virtual pixels
layout(location = 1) in vec4 inSrc;   // u, v, w, h in atlas pixels
layout(location = 2) in float inRotation;
layout(location = 3) in vec4 inTint;  // sRGB

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
} pc;

layout(set = 0, binding = 0) uniform sampler2D atlas;

layout(location = 0) out vec2 outUV;
layout(location = 1) out vec4 outTint;

const vec2 kCorners[6] = vec2[](
    vec2(0, 0), vec2(1, 0), vec2(0, 1),
    vec2(0, 1), vec2(1, 0), vec2(1, 1));

// Exact sRGB-to-linear transfer function.
vec3 srgbToLinear(vec3 c)
{
    return mix(c / 12.92, pow((c + 0.055) / 1.055, vec3(2.4)), greaterThan(c, vec3(0.04045)));
}

void main()
{
    vec2 corner = kCorners[gl_VertexIndex];
    vec2 size = inRect.zw;

    // Snap to whole pixels so sprites never shimmer, then rotate about the centre.
    vec2 centre = floor(inRect.xy + 0.5) + size * 0.5;
    vec2 local = (corner - 0.5) * size;
    float s = sin(inRotation);
    float c = cos(inRotation);
    local = vec2(c * local.x - s * local.y, s * local.x + c * local.y);

    vec2 pos = centre + local;
    gl_Position = vec4(pos / pc.screenSize * 2.0 - 1.0, 0.0, 1.0);

    outUV = (inSrc.xy + corner * inSrc.zw) / vec2(textureSize(atlas, 0));
    // Tints are authored as sRGB like the atlas; blending happens in linear space.
    outTint = vec4(srgbToLinear(inTint.rgb), inTint.a);
}
