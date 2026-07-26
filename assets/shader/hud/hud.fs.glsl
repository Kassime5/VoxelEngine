#version 460 core

in vec2 TexCoord;
out vec4 FragColor;

uniform bool useTexture;
uniform sampler2D hudTexture;
uniform vec4 color;

void main() {
    if (useTexture) {
        vec4 texColor = texture(hudTexture, TexCoord);
        FragColor = texColor * color;
    } else {
        FragColor = color;
    }
}
