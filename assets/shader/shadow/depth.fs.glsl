#version 460 core
in vec2 LocalUV;
flat in uint TileIndex;

uniform sampler2DArray ourTexture;
uniform bool alphaTested;

const float ALPHA_CUTOFF = 0.1;

void main()
{
    if (alphaTested && texture(ourTexture, vec3(LocalUV, float(TileIndex))).a < ALPHA_CUTOFF)
        discard;
}
