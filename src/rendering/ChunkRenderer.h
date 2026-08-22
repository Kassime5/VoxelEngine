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
class World;

// owns every GPU-side resource needed to draw terrain
class ChunkRenderer {
public:
    ChunkRenderer();

    ChunkRenderer(const ChunkRenderer&) = delete;
    ChunkRenderer& operator=(const ChunkRenderer&) = delete;

    bool loadTextureAtlas(const char* atlasPath, int tilesPerRow = 16);
    const TextureAtlas& getTextureAtlas() const { return textureAtlas; }

    // Culls once, then replays the surviving chunks for the opaque and transparent passes.
    void render(const World& world, const glm::mat4& projection, const glm::mat4& view, const SunState& sun);

private:
    struct VisibleChunk {
        glm::vec3 worldPos;
        const Chunk* chunk;
    };

    Shader* terrainShader;
    TextureAtlas textureAtlas;

    // Retained across frames so the per-frame cull does not reallocate.
    std::vector<VisibleChunk> visibleChunks;

    void cullChunks(const World& world, const glm::mat4& viewProj);
    void renderOpaque() const;
    void renderTransparent() const;
};

#endif //GLFWVOXEL_CHUNKRENDERER_H
