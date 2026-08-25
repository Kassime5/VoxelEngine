//
// Created by maxim on 26/07/2026.
//

#include "ChunkRenderer.h"

#include "src/core/GL.h"
#include <iostream>

#include "Profiler.h"
#include "ShaderManager.h"
#include "ShadowMap.h"
#include "src/debug/RenderStats.h"
#include "Skybox.h"
#include "src/world/Chunk.h"
#include "src/world/World.h"

ChunkRenderer::ChunkRenderer() {
    ShaderManager& sm = ShaderManager::getInstance();
    sm.addShader("terrain", "assets/shader/terrain/terrain.vs.glsl",
                 "assets/shader/terrain/terrain.fs.glsl");
    terrainShader = sm.getShader("terrain");

    sm.addShader("shadow_depth", "assets/shader/shadow/depth.vs.glsl",
                 "assets/shader/shadow/depth.fs.glsl");
    depthShader = sm.getShader("shadow_depth");
}

bool ChunkRenderer::loadTextureAtlas(const char* atlasPath, int tilesPerRow) {
    return textureAtlas.load(atlasPath, tilesPerRow);
}

void ChunkRenderer::render(const World& world, const glm::mat4& projection, const glm::mat4& view,
                           const SunState& sun, const glm::vec3& viewPos, bool underwater,
                           const Skybox& skybox, const ShadowMap& shadowMap) {
    PROFILE_FUNCTION();

    cullChunks(world, projection * view, visibleChunks);

    terrainShader->use();
    terrainShader->setInt("ourTexture", 0);

    terrainShader->setVec3("lightDir", sun.direction);
    terrainShader->setFloat("sunIntensity", sun.intensity);
    terrainShader->setVec3("lightColor", sun.color);
    terrainShader->setMat4("projection", projection);
    terrainShader->setMat4("view", view);

    terrainShader->setVec3("viewPos", viewPos);
    terrainShader->setBool("useSkyFog", !underwater);
    terrainShader->setVec3("fogColor", WATER_FOG_COLOR);

    const float edge = static_cast<float>(world.getRenderDistance() * Chunk::SIZE);
    terrainShader->setFloat("fogDensity",
        underwater ? WATER_FOG_DENSITY
                   : (edge > 0.0f ? FOG_EDGE_FALLOFF / edge : 0.0f));

    // dayBlend matches what Skybox::draw uses, so the two agree on the same sky.
    terrainShader->setInt("daySkybox", DAY_SKY_UNIT);
    terrainShader->setInt("nightSkybox", NIGHT_SKY_UNIT);
    terrainShader->setFloat("dayBlend", sun.intensity);

    glActiveTexture(GL_TEXTURE0 + DAY_SKY_UNIT);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.getDayTexture());
    glActiveTexture(GL_TEXTURE0 + NIGHT_SKY_UNIT);
    glBindTexture(GL_TEXTURE_CUBE_MAP, skybox.getNightTexture());

    // Bound even when shadows are off
    terrainShader->setInt("shadowMap", SHADOW_UNIT);
    shadowMap.bindForRead(SHADOW_UNIT);

    const int cascades = shadowMap.getCascadeCount();
    terrainShader->setInt("shadowCascades", cascades);
    terrainShader->setMat4Array("lightSpaceMatrix", shadowMap.getLightSpaceMatrices(), cascades);
    terrainShader->setFloatArray("shadowSplit", shadowMap.getSplitDistances(), cascades);

    terrainShader->setFloat("shadowStrength", shadowMap.getEffectiveStrength());
    terrainShader->setFloat("shadowTexelUV", shadowMap.getTexelUV());
    terrainShader->setFloat("shadowCascadeBlend", shadowMap.getCascadeBlend());
    terrainShader->setFloat("shadowFadeStart", shadowMap.getFadeStart());
    terrainShader->setFloat("shadowFadeEnd", shadowMap.getFadeEnd());

    textureAtlas.bind(0);

    terrainShader->setFloat("passAlpha", 1.0f);
    renderOpaque();
    renderTransparent();
    renderWater();
}

void ChunkRenderer::renderShadowPass(const World& world, ShadowMap& shadowMap) {
    PROFILE_FUNCTION();

    RenderStats::getInstance().setShadowPass(true);

    depthShader->use();
    depthShader->setInt("ourTexture", 0);
    textureAtlas.bind(0);

    for (int cascade = 0; cascade < shadowMap.getCascadeCount(); ++cascade) {
        shadowMap.beginCascade(cascade);

        // Each cascade is a different volume, so each needs its own cull against it.
        cullChunks(world, shadowMap.getLightSpaceMatrix(cascade), shadowChunks);
        depthShader->setMat4("lightSpaceMatrix", shadowMap.getLightSpaceMatrix(cascade));

        depthShader->setBool("alphaTested", true);
        for (const VisibleChunk& visible : shadowChunks) {
            depthShader->setVec3("chunkOffset", visible.worldPos);
            visible.chunk->draw();
        }
        // for grass
        glDisable(GL_CULL_FACE);

        for (const VisibleChunk& visible : shadowChunks) {
            if (visible.chunk->isTransparentMeshEmpty())
                continue;

            depthShader->setVec3("chunkOffset", visible.worldPos);
            visible.chunk->drawTransparent();
        }

        glEnable(GL_CULL_FACE);
    }

    RenderStats::getInstance().setShadowPass(false);
}

void ChunkRenderer::cullChunks(const World& world, const glm::mat4& viewProj, std::vector<VisibleChunk>& out) {
    PROFILE_SCOPE("ChunkRenderer::cullChunks");

    Frustum frustum;
    frustum.extractFromMatrix(viewProj);

    constexpr float chunkSize = static_cast<float>(Chunk::SIZE);
    constexpr float chunkHeight = static_cast<float>(Chunk::HEIGHT);

    out.clear();

    for (const auto& [chunkPos, chunk] : world.getChunks()) {
        glm::vec3 worldPos(chunkPos.x * chunkSize, chunkPos.y, chunkPos.z * chunkSize);
        glm::vec3 chunkMax = worldPos + glm::vec3(chunkSize, chunkHeight, chunkSize);

        if (!frustum.isBoxInFrustum(worldPos, chunkMax)) {
            RenderStats::getInstance().addChunkCulled();
            continue;
        }

        RenderStats::getInstance().addChunkRendered();
        out.push_back({worldPos, chunk.get()});
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

void ChunkRenderer::renderWater() const {
    PROFILE_SCOPE("ChunkRenderer::renderWater");

    terrainShader->setFloat("passAlpha", WATER_ALPHA);
    // Water still depth-tests against terrain, but must not occlude other water.
    glDepthMask(GL_FALSE);

    for (const VisibleChunk& visible : visibleChunks) {
        if (visible.chunk->isWaterMeshEmpty())
            continue;

        terrainShader->setVec3("chunkOffset", visible.worldPos);
        visible.chunk->drawWater();
    }

    glDepthMask(GL_TRUE);
    terrainShader->setFloat("passAlpha", 1.0f);
}
