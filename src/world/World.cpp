//
// Created by maxim on 03/01/2026.
//

#include "World.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

World::World()
    : m_renderDistance(8), m_lastCameraChunkPos(INT_MAX, INT_MAX, INT_MAX), seed(1010){
}

bool World::loadTextureAtlas(const char* atlasPath, int tilesPerRow) {
    return m_textureAtlas.load(atlasPath, tilesPerRow);
}

void World::update(const glm::vec3& cameraPosition) {
    glm::ivec3 currentChunkPos = worldToChunkPos(
        static_cast<int>(std::floor(cameraPosition.x)),
        static_cast<int>(std::floor(cameraPosition.y)),
        static_cast<int>(std::floor(cameraPosition.z))
    );

    if (currentChunkPos != m_lastCameraChunkPos) {
        loadChunksAroundPosition(currentChunkPos);
        unloadDistantChunks(currentChunkPos);
        m_lastCameraChunkPos = currentChunkPos;
    }

    for (auto& pair : m_chunks) {
        if (pair.second->isDirty()) {
            pair.second->generateMesh(&m_textureAtlas, this);
        }
    }
}

void World::render(Shader& shader) {
    // Bind texture atlas once for entire world
    m_textureAtlas.bind(0);

    for (auto& pair : m_chunks) {
        const glm::ivec3& chunkPos = pair.first;
        Chunk* chunk = pair.second.get();

        glm::mat4 model = glm::mat4(1.0f);
        glm::vec3 worldPos = glm::vec3(chunkPos) * static_cast<float>(Chunk::SIZE);
        model = glm::translate(model, worldPos);
        shader.setMat4("model", model);

        chunk->draw();
    }
}

BlockType World::getBlock(int worldX, int worldY, int worldZ) const {
    glm::ivec3 chunkPos = worldToChunkPos(worldX, worldY, worldZ);

    auto it = m_chunks.find(chunkPos);
    if (it == m_chunks.end()) {
        return BlockType::Air;
    }

    glm::ivec3 localPos = worldToLocalPos(worldX, worldY, worldZ);
    return it->second->getBlock(localPos.x, localPos.y, localPos.z);
}

void World::setBlock(int worldX, int worldY, int worldZ, BlockType type) {
    glm::ivec3 chunkPos = worldToChunkPos(worldX, worldY, worldZ);

    Chunk* chunk = getChunk(chunkPos);
    if (!chunk) {
        chunk = createChunk(chunkPos);
    }

    glm::ivec3 localPos = worldToLocalPos(worldX, worldY, worldZ);
    chunk->setBlock(localPos.x, localPos.y, localPos.z, type);

    // Mark neighboring chunks as dirty if block is on edge
    if (localPos.x == 0) {
        if (Chunk* neighbor = getChunk(chunkPos + glm::ivec3(-1, 0, 0))) {
            neighbor->markDirty();
        }
    } else if (localPos.x == Chunk::SIZE - 1) {
        if (Chunk* neighbor = getChunk(chunkPos + glm::ivec3(1, 0, 0))) {
            neighbor->markDirty();
        }
    }

    if (localPos.z == 0) {
        if (Chunk* neighbor = getChunk(chunkPos + glm::ivec3(0, 0, -1))) {
            neighbor->markDirty();
        }
    } else if (localPos.z == Chunk::SIZE - 1) {
        if (Chunk* neighbor = getChunk(chunkPos + glm::ivec3(0, 0, 1))) {
            neighbor->markDirty();
        }
    }
}

Chunk* World::getChunk(const glm::ivec3& chunkPos) {
    auto it = m_chunks.find(chunkPos);
    return (it != m_chunks.end()) ? it->second.get() : nullptr;
}

Chunk* World::getChunkAt(int worldX, int worldY, int worldZ) {
    glm::ivec3 chunkPos = worldToChunkPos(worldX, worldY, worldZ);
    return getChunk(chunkPos);
}

glm::ivec3 World::worldToChunkPos(int worldX, int worldY, int worldZ) const {
    return glm::ivec3(
        worldX < 0 ? (worldX + 1) / Chunk::SIZE - 1 : worldX / Chunk::SIZE,
        0,
        worldZ < 0 ? (worldZ + 1) / Chunk::SIZE - 1 : worldZ / Chunk::SIZE
    );
}

glm::ivec3 World::worldToLocalPos(int worldX, int worldY, int worldZ) const {
    int localX = worldX % Chunk::SIZE;
    int localZ = worldZ % Chunk::SIZE;

    if (localX < 0) localX += Chunk::SIZE;
    if (localZ < 0) localZ += Chunk::SIZE;

    return glm::ivec3(localX, worldY, localZ);
}

void World::loadChunksAroundPosition(const glm::ivec3& centerChunkPos) {
    for (int x = -m_renderDistance; x <= m_renderDistance; x++) {
        for (int z = -m_renderDistance; z <= m_renderDistance; z++) {
            float distance = std::sqrt(x * x + z * z);
            if (distance > m_renderDistance) continue;

            glm::ivec3 chunkPos = centerChunkPos + glm::ivec3(x, 0, z);

            if (!isChunkLoaded(chunkPos)) {
                auto chunk = std::make_unique<Chunk>(chunkPos);
                chunk->generate(&perlinNoise);
                m_chunks[chunkPos] = std::move(chunk);

                // Mark existing neighbors as dirty too
                const glm::ivec3 neighbors[] = {
                    {-1, 0, 0}, {1, 0, 0},
                    {0, 0, -1}, {0, 0, 1}
                };

                for (const auto& offset : neighbors) {
                    if (Chunk* neighbor = getChunk(chunkPos + offset)) {
                        neighbor->markDirty();
                    }
                }
            }
        }
    }
}

void World::unloadDistantChunks(const glm::ivec3& centerChunkPos) {
    std::vector<glm::ivec3> chunksToUnload;

    for (auto& pair : m_chunks) {
        const glm::ivec3& chunkPos = pair.first;

        // Calculate distance from center
        glm::ivec3 diff = chunkPos - centerChunkPos;
        float distance = std::sqrt(diff.x * diff.x + diff.z * diff.z);

        // Unload chunks beyond render distance (with some buffer)
        if (distance > m_renderDistance + 2) {
            chunksToUnload.push_back(chunkPos);
        }
    }

    const glm::ivec3 neighbors[] = {
        {-1, 0, 0}, {1, 0, 0},
        {0, 0, -1}, {0, 0, 1}
    };

    // Remove chunks
    for (const glm::ivec3& pos : chunksToUnload) {
        m_chunks.erase(pos);

        // Make neighbor chunk dirty
        for (const auto& offset : neighbors) {
            if (Chunk* neighbor = getChunk(pos + offset)) {
                neighbor->markDirty();
            }
        }
    }
}

Chunk* World::createChunk(const glm::ivec3& chunkPos) {
    auto chunk = std::make_unique<Chunk>(chunkPos);
    chunk->generate(&perlinNoise);  // Pass the noise generator

    Chunk* ptr = chunk.get();
    m_chunks[chunkPos] = std::move(chunk);

    ptr->generateMesh(&m_textureAtlas, this);

    const glm::ivec3 neighbors[] = {
        {-1, 0, 0}, {1, 0, 0},
        {0, 0, -1}, {0, 0, 1}
    };

    for (const auto& offset : neighbors) {
        if (Chunk* neighbor = getChunk(chunkPos + offset)) {
            neighbor->markDirty();
        }
    }

    return ptr;
}

bool World::isChunkLoaded(const glm::ivec3& chunkPos) const {
    return m_chunks.find(chunkPos) != m_chunks.end();
}