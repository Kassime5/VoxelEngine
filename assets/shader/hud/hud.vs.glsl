#version 460 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

uniform mat4 projection;
uniform mat4 model;
uniform vec4 uvRect;

void main() {
    gl_Position = projection * model * vec4(aPos, 0.0, 1.0);
    // Remaps the quad's 0..1 UVs onto one sprite's rect in the sheet
    TexCoord = mix(uvRect.xy, uvRect.zw, aTexCoord);
}
