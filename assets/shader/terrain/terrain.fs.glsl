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
// 1 for every pass but water, which is the only geometry that actually blends.
uniform float passAlpha;

uniform vec3 viewPos;
uniform float fogDensity;

// Above water the fog colour is the sky itself
uniform bool useSkyFog;
uniform vec3 fogColor;
uniform samplerCube daySkybox;
uniform samplerCube nightSkybox;
uniform float dayBlend;

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

    vec3 lit = texColor.rgb * lightColor * light;

    if (fogDensity > 0.0) {
        vec3 toFrag = FragPos - viewPos;
        float dist = length(toFrag);

        vec3 fogTarget = fogColor;
        float fog;

        if (useSkyFog) {
            vec3 dir = toFrag / max(dist, 1e-4);
            fogTarget = mix(texture(nightSkybox, dir).rgb, texture(daySkybox, dir).rgb, dayBlend);
            // density is tuned to hide the edge of the loaded world
            float d = dist * fogDensity;
            fog = 1.0 - exp(-d * d);
        } else {
            fog = 1.0 - exp(-dist * fogDensity);
        }

        lit = mix(lit, fogTarget, clamp(fog, 0.0, 1.0));
    }

    // Foliage stays opaque past the cutoff, so it never depends on draw order
    FragColor = vec4(lit, passAlpha);
}
