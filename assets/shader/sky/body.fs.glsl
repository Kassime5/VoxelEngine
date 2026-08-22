#version 460 core
out vec4 FragColor;

in vec2 TexCoord;

uniform sampler2D bodyTexture;
uniform vec3 tint;

void main()
{
    vec4 texel = texture(bodyTexture, TexCoord);

    // The sprites are mostly transparent
    if (texel.a < 0.01) {
        discard;
    }

    FragColor = vec4(texel.rgb * tint, texel.a);
}
