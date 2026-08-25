#version 460 core
out vec4 FragColor;

in vec2 LocalUV;
in vec3 Normal;
in vec3 FragPos;
flat in uint TileIndex;
flat in float FaceShade;

// ShadowMap::MAX_CASCADES
const int MAX_CASCADES = 4;

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

// 0 disables the lookup entirely; the renderer also fades it out near the horizon.
uniform float shadowStrength;
uniform sampler2DArrayShadow shadowMap;
uniform int shadowCascades;
uniform mat4 lightSpaceMatrix[MAX_CASCADES];
uniform float shadowSplit[MAX_CASCADES];
uniform float shadowTexelUV;
uniform float shadowCascadeBlend;
uniform float shadowFadeStart;
uniform float shadowFadeEnd;

// Floor brightness at night and at midday
const float NIGHT_AMBIENT = 0.15;
const float DAY_AMBIENT = 0.35;

const float ALPHA_CUTOFF = 0.1;

const vec2 PCF_TAPS[9] = vec2[9](
    vec2(-1.0, -1.0), vec2(0.0, -1.0), vec2(1.0, -1.0),
    vec2(-1.0,  0.0), vec2(0.0,  0.0), vec2(1.0,  0.0),
    vec2(-1.0,  1.0), vec2(0.0,  1.0), vec2(1.0,  1.0)
);

// One cascade's contribution. 1 fully lit, 0 fully shadowed.
float sampleCascade(int c, vec3 worldPos)
{
    vec4 lightPos = lightSpaceMatrix[c] * vec4(worldPos, 1.0);

    vec3 proj = lightPos.xyz / lightPos.w;
    proj = proj * 0.5 + 0.5;

    // Outside the map is simply lit; GLES 3.0 has no border colour to lean on.
    if (proj.z > 1.0 ||
        any(lessThan(proj.xy, vec2(0.0))) || any(greaterThan(proj.xy, vec2(1.0))))
        return 1.0;

    // Each tap is already hardware 2x2 PCF, so nine of them cover a 4x4-ish footprint.
    float sum = 0.0;
    for (int i = 0; i < 9; ++i) {
        sum += texture(shadowMap,
                       vec4(proj.xy + PCF_TAPS[i] * shadowTexelUV, float(c), proj.z));
    }
    return sum / 9.0;
}

// 1 fully lit, 0 fully shadowed
float sunVisibility(float viewDist, vec3 worldPos)
{
    // Smallest shell that still contains this fragment
    int c = shadowCascades - 1;
    for (int i = 0; i < MAX_CASCADES; ++i) {
        if (i < shadowCascades && viewDist < shadowSplit[i]) {
            c = i;
            break;
        }
    }

    float visibility = sampleCascade(c, worldPos);

    // Cross-fade into the next shell, or the jump in texel size reads as a ring on the ground
    float band = shadowSplit[c] * shadowCascadeBlend;
    if (c + 1 < shadowCascades && viewDist > shadowSplit[c] - band) {
        float t = clamp((viewDist - (shadowSplit[c] - band)) / band, 0.0, 1.0);
        visibility = mix(visibility, sampleCascade(c + 1, worldPos), t);
    }

    // Shadows dissolve over the last stretch of the outermost cascade
    float edgeFade = smoothstep(shadowFadeStart, shadowFadeEnd, viewDist);
    return mix(visibility, 1.0, edgeFade);
}

void main()
{
    vec4 texColor = texture(ourTexture, vec3(LocalUV, float(TileIndex)));

    if (texColor.a < ALPHA_CUTOFF)
        discard;

    vec3 toFrag = FragPos - viewPos;
    float dist = length(toFrag);

    // Directional sun, faded by how far above the horizon it is
    vec3 norm = normalize(Normal);
    float NdotL = max(dot(norm, lightDir), 0.0);
    float diff = NdotL * sunIntensity;

    // Only the direct term is occluded -- shadowing the ambient as well turns faces black
    if (shadowStrength > 0.0 && diff > 0.0) {
        diff *= mix(1.0, sunVisibility(dist, FragPos), shadowStrength);
    }

    float ambient = mix(NIGHT_AMBIENT, DAY_AMBIENT, sunIntensity);
    float light = (ambient + (1.0 - ambient) * diff) * FaceShade;

    vec3 lit = texColor.rgb * lightColor * light;

    if (fogDensity > 0.0) {
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
