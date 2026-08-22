//
// Created by maxim on 03/01/2026.
//

#ifndef GLFWVOXEL_WORLD_H
#define GLFWVOXEL_WORLD_H

#include "Block.h"
#include "Chunk.h"
#include "../rendering/TextureAltas.h"
#include <unordered_map>
#include <memory>
#include <vector>
#include <queue>
#include <chrono>
#include <climits>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include "WorleyBiome.h"
#include "PerlinNoise/PerlinNoise.hpp"
#include "../rendering/Profiler.h"
#include "src/game/entities/EntityManager.h"
#include "src/utils/VectorHash.h"

class Player;

struct ChunkGenerationTask {
    glm::ivec3 chunkPos;
    Chunk* chunk;
};

struct ChunkMeshTask {
    Chunk* chunk;
    MeshData meshData;
    MeshData transparentMeshData;
};

struct RaycastResult {
    bool hit;
    glm::vec3 hitPos;
    glm::vec3 hitNormal;
};

using ChunkMap = std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, IVec3Hash>;

class World {
public:
    // textureAtlas is borrowed, not owned -- it belongs to ChunkRenderer and must
    // outlive this World. Mesh worker threads read UVs from it.
    World(Player& _player, const TextureAtlas& _textureAtlas);
    ~World();

    void update(const glm::vec3& cameraPosition);

    const ChunkMap& getChunks() const { return m_chunks; }

    BlockType getBlock(int worldX, int worldY, int worldZ);
    void setBlock(int worldX, int worldY, int worldZ, BlockType type);

    Chunk* getChunk(const glm::ivec3& chunkPos);
    Chunk* getChunkAt(int worldX, int worldY, int worldZ);

    // Invalidating the cached chunk position is the point: update() only reloads chunks
    // when the player crosses a chunk boundary, so without this a new render distance did
    // nothing at all until they happened to walk into the next chunk.
    void setRenderDistance(int distance) {
        if (distance == renderDistance) {
            return;
        }
        renderDistance = distance;
        lastCameraChunkPos = glm::ivec3(INT_MAX, INT_MAX, INT_MAX);
    }
    int getRenderDistance() const { return renderDistance; }
    int getLoadedChunkCount() const { return m_chunks.size(); }

    using SeedType = siv::PerlinNoise::seed_type;

    // Throws away every chunk and entity and rebuilds the world from a new seed
    void regenerate(SeedType newSeed);
    void regenerate() { regenerate(randomSeed()); }

    SeedType getSeed() const { return seed; }
    static SeedType randomSeed();
    const Biome* getCurrentPlayerBiome(float cameraX, float cameraZ) const;
    bool hasTerrainAt(int worldX, int worldZ);
    RaycastResult raycastBlock(const glm::vec3& origin, const glm::vec3& direction, float maxDistance = 10.0f);

    EntityManager* getEntityManager() { return &entityManager; }

private:
    Player& player;

    ChunkMap m_chunks;
    const TextureAtlas& textureAtlas;
    int renderDistance;
    glm::ivec3 lastCameraChunkPos;

    // Thread pool
    std::vector<std::jthread> generationThreads;
    std::queue<ChunkGenerationTask> generationQueue;
    std::mutex generationQueueMutex;
    std::condition_variable_any generationQueueCV;

    // Thread pool for mesh building
    std::vector<std::jthread> meshBuildThreads;
    std::queue<ChunkMeshTask> meshBuildQueue;
    std::mutex meshBuildQueueMutex;
    std::condition_variable_any meshBuildQueueCV;

    // GPU upload queue (processed on main thread)
    std::queue<ChunkMeshTask> gpuUploadQueue;
    std::mutex gpuUploadQueueMutex;

    std::queue<glm::ivec3> pendingEntitySpawns;
    std::mutex pendingEntitySpawnsMutex;
    void processPendingEntitySpawns(int maxPerFrame);

    void initThreadPool(int _generationThreads, int _meshThreads);
    void shutdownThreadPool();
    void generationWorkerThread(std::stop_token stopToken);
    void meshBuildWorkerThread(std::stop_token stopToken);
    void processGPUUploadQueue(int maxPerFrame);
    bool processOneGenerationTask();
    bool processOneMeshBuildTask();
    void pumpChunkWork(std::chrono::microseconds budget);

    // Noise. Not const: regenerate() reseeds them in place rather than rebuilding the
    // World, which would dangle the World* and World& that PlayerController and the debug
    // UI hold onto.
    siv::PerlinNoise::seed_type seed;
    siv::PerlinNoise perlinNoise;
    std::unique_ptr<WorleyBiome> worleyBiome;

    glm::ivec3 worldToChunkPos(int worldX, int worldY, int worldZ) const;
    glm::ivec3 worldToLocalPos(int worldX, int worldY, int worldZ) const;

    void loadChunksAroundPosition(const glm::ivec3& centerChunkPos);
    void unloadDistantChunks(const glm::ivec3& centerChunkPos);
    bool isChunkLoaded(const glm::ivec3& chunkPos) const;

    EntityManager entityManager;
};

#endif //GLFWVOXEL_WORLD_H