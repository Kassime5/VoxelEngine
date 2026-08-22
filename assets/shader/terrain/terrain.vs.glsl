#version 460 core
const vec3 NORMALS[6] = vec3[6](
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 0.0, -1.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, -1.0, 0.0),
    vec3(1.0, 0.0, 0.0),
    vec3(-1.0, 0.0, 0.0)
);

// Fixed per-face brightness, the trick Minecraft uses. Two faces of the same block never
// share a shade, so block edges stay readable even where the sun reaches neither of them.
// Indices match NORMALS above.
const float FACE_SHADE[6] = float[6](
    0.8, 0.8,   // +Z / -Z
    1.0,        // +Y, top
    0.5,        // -Y, bottom
    0.6, 0.6    // +X / -X
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
layout (location = 4) in uint quadWidth;
layout (location = 5) in uint quadHeight;

out vec3 FragPos;
out vec3 Normal;
flat out uint TileIndex;
out vec2 LocalUV;
// Constant across a face, so flat -- there is nothing to interpolate. Per-vertex ambient
// occlusion, when it arrives, wants a separate non-flat varying alongside this.
flat out float FaceShade;

uniform vec3 chunkOffset;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec3 worldPos = aPos + chunkOffset;
    gl_Position = projection * view * vec4(worldPos, 1.0);

    // Local UVs scaled by quad dimensions; the sampler's GL_REPEAT tiles them
    vec2 cornerOffset = CORNER_UVS[cornerIndex];
    LocalUV = cornerOffset * vec2(float(quadWidth), float(quadHeight));

    // Atlas array layer for this face
    TileIndex = tileIndex;

    FragPos = worldPos;
    Normal = NORMALS[normalId];
    FaceShade = FACE_SHADE[normalId];
}
