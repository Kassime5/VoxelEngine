#version 460 core
// Same vertex layout as terrain
const vec2 CORNER_UVS[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

layout (location = 0) in vec3 aPos;
layout (location = 1) in uint tileIndex;
layout (location = 2) in uint cornerIndex;
layout (location = 4) in uint quadWidth;
layout (location = 5) in uint quadHeight;

out vec2 LocalUV;
flat out uint TileIndex;

uniform vec3 chunkOffset;
uniform mat4 lightSpaceMatrix;

void main()
{
    gl_Position = lightSpaceMatrix * vec4(aPos + chunkOffset, 1.0);

    LocalUV = CORNER_UVS[cornerIndex] * vec2(float(quadWidth), float(quadHeight));
    TileIndex = tileIndex;
}
