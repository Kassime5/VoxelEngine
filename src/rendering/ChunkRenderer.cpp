//
// Created by maxim on 26/07/2026.
//

#include "ChunkRenderer.h"

#include "src/core/GL.h"
#include <iostream>

#include "Profiler.h"
#include "ShaderManager.h"
#include "src/debug/RenderStats.h"
#include "src/world/Chunk.h"
#include "src/world/World.h"

ChunkRenderer::ChunkRenderer() {
    ShaderManager& sm = ShaderManager::getInstance();
    sm.addShader("terrain", "assets/shader/terrain/terrain.vs.glsl",
                 "assets/shader/terrain/terrain.fs.glsl");
    terrainShader = sm.getShader("terrain");
}

bool ChunkRenderer::loadTextureAtlas(const char* atlasPath, int tilesPerRow) {
    return textureAtlas.load(atlasPath, tilesPerRow);
}

void ChunkRenderer::render(const World& world, const glm::mat4& projection, const glm::mat4& view,
                           const SunState& sun) {
    PROFILE_FUNCTION();

    cullChunks(world, projection * view);

    terrainShader->use();
    terrainShader->setInt("ourTexture", 0);

    terrainShader->setVec3("lightDir", sun.direction);
    terrainShader->setFloat("sunIntensity", sun.intensity);
    terrainShader->setVec3("lightColor", sun.color);
    terrainShader->setMat4("projection", projection);
    terrainShader->setMat4("view", view);

    textureAtlas.bind(0);

    renderOpaque();
    renderTransparent();
}

void ChunkRenderer::cullChunks(const World& world, const glm::mat4& viewProj) {
    PROFILE_SCOPE("ChunkRenderer::cullChunks");

    Frustum frustum;
    frustum.extractFromMatrix(viewProj);

    constexpr float chunkSize = static_cast<float>(Chunk::SIZE);
    constexpr float chunkHeight = static_cast<float>(Chunk::HEIGHT);

    visibleChunks.clear();

    for (const auto& [chunkPos, chunk] : world.getChunks()) {
        glm::vec3 worldPos(chunkPos.x * chunkSize, chunkPos.y, chunkPos.z * chunkSize);
        glm::vec3 chunkMax = worldPos + glm::vec3(chunkSize, chunkHeight, chunkSize);

        if (!frustum.isBoxInFrustum(worldPos, chunkMax)) {
            RenderStats::getInstance().addChunkCulled();
            continue;
        }

        RenderStats::getInstance().addChunkRendered();
        visibleChunks.push_back({worldPos, chunk.get()});
    }
}

void ChunkRenderer::renderOpaque() const {
    PROFILE_SCOPE("ChunkRenderer::renderOpaque");

    for (const VisibleChunk& visible : visibleChunks) {
        terrainShader->setVec3("chunkOffset", visible.worldPos);
        visible.chunk->draw();
    }
}

void ChunkRenderer::renderTransparent() const {
    PROFILE_SCOPE("ChunkRenderer::renderTransparent");

    // cross models are two-sided, so culling must be off
    glDisable(GL_CULL_FACE);

    for (const VisibleChunk& visible : visibleChunks) {
        if (visible.chunk->isTransparentMeshEmpty())
            continue;

        terrainShader->setVec3("chunkOffset", visible.worldPos);
        visible.chunk->drawTransparent();
    }

    glEnable(GL_CULL_FACE);
}
