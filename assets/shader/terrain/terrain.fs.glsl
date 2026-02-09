#version 460 core
out vec4 FragColor;

in vec2 LocalUV;
in vec3 Normal;
in vec3 FragPos;
flat in uint TileIndex;

uniform sampler2D ourTexture;
uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform float tilesPerRow;

void main()
{
    // Normalize LocalUV to 0-1 range for a single tile
    vec2 normalizedUV = fract(LocalUV);

    // Calculate tile position in atlas
    float tileX = float(TileIndex % uint(tilesPerRow));
    float tileY = float(TileIndex / uint(tilesPerRow));

    // Calculate final UV within the atlas
    float tileSize = 1.0 / tilesPerRow;

    vec2 TexCoord = vec2(
        tileX * tileSize + normalizedUV.x * tileSize,
        tileY * tileSize + normalizedUV.y * tileSize
    );

    // ambient
    float ambientStrength = 0.2;
    vec3 ambient = ambientStrength * lightColor;

    // diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // specular
//     float specularStrength = 0.5;
//     vec3 viewDir = normalize(viewPos - FragPos);
//     vec3 reflectDir = reflect(-lightDir, norm);
//     float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
//     vec3 specular = specularStrength * spec * lightColor;

//     vec3 result = ambient + diffuse + specular;
    vec3 result = ambient + diffuse;
    FragColor = texture(ourTexture, TexCoord) * vec4(result, 1.0);
}