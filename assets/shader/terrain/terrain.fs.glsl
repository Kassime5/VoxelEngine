#version 460 core
out vec4 FragColor;

in vec2 LocalUV;
in vec3 Normal;
in vec3 FragPos;
flat in uint TileIndex;
flat in float FaceShade;

uniform sampler2DArray ourTexture;
uniform vec3 lightDir;
uniform vec3 lightColor;
// 0 through the night, 1 with the sun up. Fades across the horizon.
uniform float sunIntensity;

// Floor brightness at night and at midday
const float NIGHT_AMBIENT = 0.15;
const float DAY_AMBIENT = 0.35;

const float ALPHA_CUTOFF = 0.1;

void main()
{
    vec4 texColor = texture(ourTexture, vec3(LocalUV, float(TileIndex)));

    if (texColor.a < ALPHA_CUTOFF)
        discard;

    // Directional sun, faded by how far above the horizon it is
    vec3 norm = normalize(Normal);
    float diff = max(dot(norm, lightDir), 0.0) * sunIntensity;
    float ambient = mix(NIGHT_AMBIENT, DAY_AMBIENT, sunIntensity);
    float light = (ambient + (1.0 - ambient) * diff) * FaceShade;

    // Opaque past the cutoff, so foliage never depends on draw order
    FragColor = vec4(texColor.rgb * lightColor * light, 1.0);
}
