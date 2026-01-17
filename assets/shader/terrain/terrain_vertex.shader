#version 460 core
const vec3 NORMALS[6] = vec3[6](
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 0.0, -1.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, -1.0, 0.0),
    vec3(1.0, 0.0, 0.0),
    vec3(-1.0, 0.0, 0.0)
);

const vec2 CORNER_UVS[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

layout (location = 0) in vec3 aPos;
layout (location = 1) in uint tileIndex;
layout (location = 2) in uint cornerIndex;
layout (location = 3) in uint normalId;

out vec2 TexCoord;
out vec3 FragPos;
out vec3 Normal;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform float tilesPerRow;

void main()
{
    gl_Position = projection * view * model * vec4(aPos, 1.0);

    // Calculate tile position in atlas
    float tileX = float(tileIndex % uint(tilesPerRow));
    float tileY = float(tileIndex / uint(tilesPerRow));

    // Calculate UV within the atlas
    float tileSize = 1.0 / tilesPerRow;
    vec2 cornerOffset = CORNER_UVS[cornerIndex];
    TexCoord = vec2(
        (tileX + cornerOffset.x) * tileSize,
        (tileY + cornerOffset.y) * tileSize
    );

    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = NORMALS[normalId];
}