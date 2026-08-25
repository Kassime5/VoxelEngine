#version 460 core
out vec4 FragColor;

in vec3 FragPos;
in float Shade;

uniform vec3 viewPos;
uniform vec3 lightColor;
// 0 through the night, 1 with the sun up
uniform float sunIntensity;
uniform float opacity;

// Horizontal distances over which the layer dissolves
uniform float fadeStart;
uniform float fadeEnd;

const vec3 CLOUD_COLOR = vec3(1.0);
const float NIGHT_AMBIENT = 0.35;

void main() {
    // Horizontal only, so looking straight up never fades
    float dist = length(FragPos.xz - viewPos.xz);
    float alpha = opacity * (1.0 - smoothstep(fadeStart, fadeEnd, dist));

    if (alpha < 0.01)
        discard;

    float light = mix(NIGHT_AMBIENT, 1.0, sunIntensity) * Shade;
    FragColor = vec4(CLOUD_COLOR * lightColor * light, alpha);
}
