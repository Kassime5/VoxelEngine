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
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <glm/glm.hpp>
#include "PerlinNoise/PerlinNoise.hpp"
#include "../rendering/Profiler.h"

struct IVec3Hash {
    std::size_t operator()(const glm::ivec3& v) const {
        std::size_t h1 = std::hash<int>()(v.x);
        std::size_t h2 = std::hash<int>()(v.y);
        std::size_t h3 = std::hash<int>()(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

struct ChunkGenerationTask {
    glm::ivec3 chunkPos;
    Chunk* chunk;
};

struct ChunkMeshTask {
    Chunk* chunk;
    MeshData meshData;
};

class World {
public:
    World();
    ~World();

    bool loadTextureAtlas(const char* atlasPath, int tilesPerRow = 16);
    void update(const glm::vec3& cameraPosition);
    void render(Shader& shader);

    BlockType getBlock(int worldX, int worldY, int worldZ);
    void setBlock(int worldX, int worldY, int worldZ, BlockType type);

    Chunk* getChunk(const glm::ivec3& chunkPos);
    Chunk* getChunkAt(int worldX, int worldY, int worldZ);

    void setRenderDistance(int distance) { renderDistance = distance; }
    int getRenderDistance() const { return renderDistance; }
    int getLoadedChunkCount() const { return m_chunks.size(); }
    void printDebugInfo() const;

private:
    std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, IVec3Hash> m_chunks;
    TextureAtlas textureAtlas;
    int renderDistance;
    glm::ivec3 lastCameraChunkPos;

    // Thread pool
    std::vector<std::thread> generationThreads;
    std::queue<ChunkGenerationTask> generationQueue;
    std::mutex generationQueueMutex;
    std::condition_variable generationQueueCV;

    // Thread pool for mesh building
    std::vector<std::thread> meshBuildThreads;
    std::queue<ChunkMeshTask> meshBuildQueue;
    std::mutex meshBuildQueueMutex;
    std::condition_variable meshBuildQueueCV;

    // GPU upload queue (processed on main thread)
    std::queue<ChunkMeshTask> gpuUploadQueue;
    std::mutex gpuUploadQueueMutex;

    std::atomic<bool> stopThreads;

    void initThreadPool(int _generationThreads, int _meshThreads);
    void shutdownThreadPool();
    void generationWorkerThread();
    void meshBuildWorkerThread();
    void processGPUUploadQueue(int maxPerFrame);

    // Noise
    const siv::PerlinNoise::seed_type seed;
    const siv::PerlinNoise perlinNoise;

    glm::ivec3 worldToChunkPos(int worldX, int worldY, int worldZ) const;
    glm::ivec3 worldToLocalPos(int worldX, int worldY, int worldZ) const;

    void loadChunksAroundPosition(const glm::ivec3& centerChunkPos);
    void unloadDistantChunks(const glm::ivec3& centerChunkPos);

    // Chunk* createChunk(const glm::ivec3& chunkPos);
    bool isChunkLoaded(const glm::ivec3& chunkPos) const;
};

#endif //GLFWVOXEL_WORLD_H