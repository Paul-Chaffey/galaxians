#version 450

layout(set = 0, binding = 0) uniform sampler2D atlas;

layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inTint;

layout(location = 0) out vec4 outColor;

void main()
{
    outColor = texture(atlas, inUV) * inTint;
}
