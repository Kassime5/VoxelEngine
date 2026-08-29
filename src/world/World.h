//
// Created by maxim on 03/01/2026.
//

#ifndef GLFWVOXEL_WORLD_H
#define GLFWVOXEL_WORLD_H

#include "Block.h"
#include "Chunk.h"
#include "../rendering/TextureAltas.h"
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <deque>
#include <random>
#include <unordered_set>
#include <chrono>
#include <climits>
#include <thread>
#include <mutex>
#include <shared_mutex>
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
    std::shared_ptr<Chunk> chunk;
};

struct ChunkMeshTask {
    std::shared_ptr<Chunk> chunk;
    MeshData meshData;
    MeshData transparentMeshData;
    MeshData waterMeshData;
    // The chunk's edit version when this build started
    std::uint32_t editVersion = 0;
    // Player edits take priority
    bool playerEdit = false;
};

// Who asked for a block change
enum class BlockChangeSource : std::uint8_t {
    Player,
    WorldUpdate
};

struct RaycastResult {
    bool hit;
    glm::vec3 hitPos;
    glm::vec3 hitNormal;
};

using ChunkMap = std::unordered_map<glm::ivec3, std::shared_ptr<Chunk>, IVec3Hash>;

class World {
public:
    // textureAtlas is borrowed, not owned -- it belongs to ChunkRenderer and must
    // outlive this World. Mesh worker threads read UVs from it.
    World(Player& _player, const TextureAtlas& _textureAtlas);
    ~World();

    void update(float deltaTime, const glm::vec3& cameraPosition);

    const ChunkMap& getChunks() const { return m_chunks; }

    BlockType getBlock(int worldX, int worldY, int worldZ);
    void setBlock(int worldX, int worldY, int worldZ, BlockType type);
    void setBlockLogic(const glm::ivec3& pos, BlockType type);

    // fires when a block change is actually committed
    using BlockChangeCallback = std::function<void(const glm::ivec3& pos, BlockType from, BlockType to, BlockChangeSource source)>;
    void setBlockChangeCallback(BlockChangeCallback callback) { onBlockChange = std::move(callback); }

    Chunk* getChunk(const glm::ivec3& chunkPos);
    Chunk* getChunkAt(int worldX, int worldY, int worldZ);

    ChunkNeighbourhood snapshotNeighbourhood(const glm::ivec3& chunkPos) const;

    // Invalidating the cached chunk position to trigger a chunk reload
    void setRenderDistance(int distance) {
        if (distance == renderDistance) {
            return;
        }
        renderDistance = distance;
        lastCameraChunkPos = glm::ivec3(INT_MAX, INT_MAX, INT_MAX);
    }
    int getRenderDistance() const { return renderDistance; }
    int getLoadedChunkCount() const {
        std::shared_lock lock(chunksMutex);
        return m_chunks.size();
    }

    using SeedType = siv::PerlinNoise::seed_type;

    // Throws away every chunk and entity and rebuilds the world from a new seed
    void regenerate(SeedType newSeed);
    void regenerate() { regenerate(randomSeed()); }

    SeedType getSeed() const { return seed; }
    static SeedType randomSeed();
    static SeedType seedFromString(const std::string& text);
    const Biome* getCurrentPlayerBiome(float cameraX, float cameraZ) const;
    bool hasTerrainAt(int worldX, int worldZ);
    RaycastResult raycastBlock(const glm::vec3& origin, const glm::vec3& direction, float maxDistance = 10.0f);

    EntityManager* getEntityManager() { return &entityManager; }

private:
    Player& player;

    ChunkMap m_chunks;

    mutable std::shared_mutex chunksMutex;

    const TextureAtlas& textureAtlas;
    int renderDistance;
    glm::ivec3 lastCameraChunkPos;

    // Thread pool
    std::vector<std::jthread> generationThreads;
    std::queue<ChunkGenerationTask> generationQueue;
    std::mutex generationQueueMutex;
    std::condition_variable_any generationQueueCV;

    // Thread pool for mesh building. Both queues share one mutex and CV; the edit queue is
    // drained first so a placed block never waits behind a render-distance worth of terrain.
    std::vector<std::jthread> meshBuildThreads;
    std::queue<ChunkMeshTask> meshBuildQueue;
    std::queue<ChunkMeshTask> editMeshBuildQueue;
    std::mutex meshBuildQueueMutex;
    std::condition_variable_any meshBuildQueueCV;

    // GPU upload queues (processed on main thread), same priority split
    std::queue<ChunkMeshTask> gpuUploadQueue;
    std::queue<ChunkMeshTask> editGpuUploadQueue;
    std::mutex gpuUploadQueueMutex;

    std::queue<glm::ivec3> pendingEntitySpawns;
    std::mutex pendingEntitySpawnsMutex;
    void processPendingEntitySpawns(int maxPerFrame);

    void initThreadPool(int _generationThreads, int _meshThreads);
    void shutdownThreadPool();
    void generationWorkerThread(std::stop_token stopToken);
    void meshBuildWorkerThread(std::stop_token stopToken);
    void processGPUUploadQueue(std::chrono::microseconds budget);
    bool processOneGenerationTask();
    bool processOneMeshBuildTask();
    void pumpChunkWork(std::chrono::microseconds budget);

    siv::PerlinNoise::seed_type seed;
    siv::PerlinNoise perlinNoise;
    std::unique_ptr<WorleyBiome> worleyBiome;

    glm::ivec3 worldToChunkPos(int worldX, int worldY, int worldZ) const;
    glm::ivec3 worldToLocalPos(int worldX, int worldY, int worldZ) const;

    std::vector<std::shared_ptr<Chunk>> chunkGraveyard;
    void reapUnloadedChunks();

    void loadChunksAroundPosition(const glm::ivec3& centerChunkPos);
    void unloadDistantChunks(const glm::ivec3& centerChunkPos);
    bool isChunkLoaded(const glm::ivec3& chunkPos) const;

    // A chunk meshes its borders against whatever its neighbours held at the time, so those
    // borders go stale when a neighbour arrives or a seam block changes. Main thread only.
    std::shared_ptr<Chunk> getChunkShared(const glm::ivec3& chunkPos) const;
    void queueRemesh(std::shared_ptr<Chunk> chunk, bool playerEdit);
    void collectStaleNeighbours(const glm::ivec3& chunkPos,
                                std::vector<std::shared_ptr<Chunk>>& out);

    void applyBlockChange(const glm::ivec3& pos, BlockType type, BlockChangeSource source);
    bool isSpaceOccupied(const glm::ivec3& pos) const;
    void queueSeamRemesh(const glm::ivec3& chunkPos, const glm::ivec3& localPos);

    // Block updates. Main thread only, same as the remesh queues they feed.
    struct PendingBlockUpdate {
        glm::ivec3 pos;
        std::uint8_t depth;
    };

    std::deque<PendingBlockUpdate> blockUpdateQueue;
    std::unordered_set<glm::ivec3, IVec3Hash> blockUpdateQueued;
    float blockTickAccumulator = 0.0f;
    // Depth of the update being processed, so edits a rule makes inherit it
    int currentUpdateDepth = 0;
    std::mt19937 blockTickRng{std::random_device{}()};

    void scheduleBlockUpdate(const glm::ivec3& pos, int depth);
    void scheduleNeighbourUpdates(const glm::ivec3& pos);
    void tickBlockUpdates();
    void runRandomTicks(const glm::ivec3& centerChunkPos);

    EntityManager entityManager;

    BlockChangeCallback onBlockChange;
};

#endif //GLFWVOXEL_WORLD_H