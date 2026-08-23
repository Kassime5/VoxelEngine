#version 460 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2DArray tileTexture;
uniform int tileLayer;
uniform vec4 color;

void main() {
    FragColor = texture(tileTexture, vec3(TexCoord, float(tileLayer))) * color;
}
