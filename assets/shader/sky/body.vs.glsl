#version 460 core

layout (location = 0) in vec2 aCorner;

out vec2 TexCoord;

uniform mat4 view;
uniform mat4 projection;
uniform vec3 bodyCenter;
uniform vec3 bodyRight;
uniform vec3 bodyUp;

// Just inside the far plane rather than exactly on it
const float BODY_DEPTH = 0.99999;

void main()
{
    vec3 worldPos = bodyCenter + bodyRight * aCorner.x + bodyUp * aCorner.y;

    vec4 pos = projection * view * vec4(worldPos, 1.0);
    gl_Position = vec4(pos.xy, pos.w * BODY_DEPTH, pos.w);

    TexCoord = vec2(aCorner.x * 0.5 + 0.5, 0.5 - aCorner.y * 0.5);
}
