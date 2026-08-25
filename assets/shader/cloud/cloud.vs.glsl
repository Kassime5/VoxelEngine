#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in float aShade;

uniform mat4 viewProj;
uniform vec3 cloudOffset;

out vec3 FragPos;
out float Shade;

void main() {
    FragPos = aPos + cloudOffset;
    Shade = aShade;
    gl_Position = viewProj * vec4(FragPos, 1.0);
}
