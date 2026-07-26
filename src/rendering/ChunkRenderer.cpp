//
// Created by maxim on 26/07/2026.
//

#include "ChunkRenderer.h"

#include <glad/glad.h>
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

void ChunkRenderer::render(const World& world, const glm::mat4& projection, const glm::mat4& view) {
    PROFILE_FUNCTION();

    cullChunks(world, projection * view);

    terrainShader->use();
    terrainShader->setFloat("tilesPerRow", static_cast<float>(textureAtlas.getTilesPerRow()));
    terrainShader->setInt("texture1", 0);

    // TODO: Change where the lightsource is
    terrainShader->setVec3("lightPos", glm::vec3(0.0f, 50.0f, 0.0f));
    terrainShader->setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
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

    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    for (const VisibleChunk& visible : visibleChunks) {
        if (visible.chunk->isTransparentMeshEmpty())
            continue;

        terrainShader->setVec3("chunkOffset", visible.worldPos);
        visible.chunk->drawTransparent();
    }

    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
}
