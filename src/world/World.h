//
// Created by maxim on 03/01/2026.
//

#ifndef GLFWVOXEL_WORLD_H
#define GLFWVOXEL_WORLD_H

#include "Block.h"
#include "Chunk.h"
#include "../rendering/Shader.h"
#include "../rendering/TextureAltas.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <glm/glm.hpp>

struct IVec3Hash {
    std::size_t operator()(const glm::ivec3& v) const {
        std::size_t h1 = std::hash<int>()(v.x);
        std::size_t h2 = std::hash<int>()(v.y);
        std::size_t h3 = std::hash<int>()(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class World {
public:
    World();
    ~World() = default;

    bool loadTextureAtlas(const char* atlasPath, int tilesPerRow = 16);
    void update(const glm::vec3& cameraPosition);
    void render(Shader& shader);

    BlockType getBlock(int worldX, int worldY, int worldZ) const;
    void setBlock(int worldX, int worldY, int worldZ, BlockType type);

    Chunk* getChunk(const glm::ivec3& chunkPos);
    Chunk* getChunkAt(int worldX, int worldY, int worldZ);

    void setRenderDistance(int distance) { m_renderDistance = distance; }
    int getRenderDistance() const { return m_renderDistance; }
    int getLoadedChunkCount() const { return m_chunks.size(); }

private:
    std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, IVec3Hash> m_chunks;
    TextureAtlas m_textureAtlas;
    int m_renderDistance;
    glm::ivec3 m_lastCameraChunkPos;

    glm::ivec3 worldToChunkPos(int worldX, int worldY, int worldZ) const;
    glm::ivec3 worldToLocalPos(int worldX, int worldY, int worldZ) const;

    void loadChunksAroundPosition(const glm::ivec3& centerChunkPos);
    void unloadDistantChunks(const glm::ivec3& centerChunkPos);

    Chunk* createChunk(const glm::ivec3& chunkPos);
    bool isChunkLoaded(const glm::ivec3& chunkPos) const;
};

#endif //GLFWVOXEL_WORLD_H