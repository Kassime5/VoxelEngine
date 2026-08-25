//
// Created by maxim on 26/07/2026.
//

#ifndef GLFWVOXEL_CHUNKRENDERER_H
#define GLFWVOXEL_CHUNKRENDERER_H

#include <glm/glm.hpp>
#include <vector>

#include "Frustum.h"
#include "TextureAltas.h"
#include "src/world/DayCycle.h"

class Chunk;
class Shader;
class ShadowMap;
class Skybox;
class World;

// owns every GPU-side resource needed to draw terrain
class ChunkRenderer {
public:
    ChunkRenderer();

    ChunkRenderer(const ChunkRenderer&) = delete;
    ChunkRenderer& operator=(const ChunkRenderer&) = delete;

    inline static const glm::vec3 WATER_FOG_COLOR{0.10f, 0.35f, 0.55f};
    static constexpr float WATER_TINT_ALPHA = 0.55f;

    bool loadTextureAtlas(const char* atlasPath, int tilesPerRow = 16);
    const TextureAtlas& getTextureAtlas() const { return textureAtlas; }

    // Culls once, then replays the surviving chunks for the opaque, transparent and water passes.
    void render(const World& world, const glm::mat4& projection, const glm::mat4& view,
                const SunState& sun, const glm::vec3& viewPos, bool underwater,
                const Skybox& skybox, const ShadowMap& shadowMap);

    // Depth-only pass into the shadow map
    void renderShadowPass(const World& world, ShadowMap& shadowMap);

private:
    static constexpr float WATER_ALPHA = 0.7f;
    static constexpr float WATER_FOG_DENSITY = 0.04f;
    static constexpr float FOG_EDGE_FALLOFF = 1.5f;
    static constexpr int DAY_SKY_UNIT = 2;
    static constexpr int NIGHT_SKY_UNIT = 3;
    static constexpr int SHADOW_UNIT = 4;

    struct VisibleChunk {
        glm::vec3 worldPos;
        const Chunk* chunk;
    };

    Shader* terrainShader;
    Shader* depthShader;
    TextureAtlas textureAtlas;

    // Retained across frames so the per-frame cull does not reallocate.
    std::vector<VisibleChunk> visibleChunks;
    std::vector<VisibleChunk> shadowChunks;

    void cullChunks(const World& world, const glm::mat4& viewProj,
                    std::vector<VisibleChunk>& out);
    void renderOpaque() const;
    void renderTransparent() const;
    void renderWater() const;
};

#endif //GLFWVOXEL_CHUNKRENDERER_H
