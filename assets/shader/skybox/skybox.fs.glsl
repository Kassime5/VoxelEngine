#version 460 core
out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube daySkybox;
uniform samplerCube nightSkybox;
uniform float dayBlend;

void main()
{
    vec3 day = texture(daySkybox, TexCoords).rgb;
    vec3 night = texture(nightSkybox, TexCoords).rgb;
    FragColor = vec4(mix(night, day, dayBlend), 1.0);
}
