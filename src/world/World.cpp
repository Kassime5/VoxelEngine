//
// Created by maxim on 03/01/2026.
//

#include "World.h"
#include "src/game/Player.h"

#include <limits>
#include <random>

namespace {
#ifdef GLFWVOXEL_SINGLE_THREADED
    // to avoid hangs on single threads
    constexpr std::chrono::microseconds CHUNK_WORK_BUDGET{6000};
#endif
}

World::SeedType World::randomSeed() {
    std::random_device rd;
    return static_cast<SeedType>(rd());
}

World::World(Player& _player, const TextureAtlas& _textureAtlas)
    : player(_player), textureAtlas(_textureAtlas), renderDistance(8),
      lastCameraChunkPos(INT_MAX, INT_MAX, INT_MAX), seed(randomSeed()), perlinNoise(seed)
{
    worleyBiome = std::make_unique<WorleyBiome>(seed, 384);
#ifndef GLFWVOXEL_SINGLE_THREADED
    initThreadPool(6, 6);
#endif
}

void World::regenerate(SeedType newSeed) {
    // Order matters
    shutdownThreadPool();

    {
        std::lock_guard<std::mutex> lock(pendingEntitySpawnsMutex);
        pendingEntitySpawns = {};
    }

    m_chunks.clear();
    entityManager.clear();

    seed = newSeed;
    perlinNoise.reseed(seed);
    worleyBiome = std::make_unique<WorleyBiome>(seed, 384);

    // update() only reloads chunks when the player crosses a chunk boundary, so the cached position has to be invalidated
    lastCameraChunkPos = glm::ivec3(INT_MAX, INT_MAX, INT_MAX);

#ifndef GLFWVOXEL_SINGLE_THREADED
    initThreadPool(6, 6);
#endif
}

World::~World() {
    shutdownThreadPool();
}

void World::initThreadPool(int _generationThreads, int _meshThreads) {
    for (int i = 0; i < _generationThreads; i++) {
        generationThreads.emplace_back([this](std::stop_token stopToken) {
            generationWorkerThread(std::move(stopToken));
        });
    }
    for (int i = 0; i < _meshThreads; i++) {
        meshBuildThreads.emplace_back([this](std::stop_token stopToken) {
            meshBuildWorkerThread(std::move(stopToken));
        });
    }
}

void World::shutdownThreadPool() {
    for (auto& thread : generationThreads) thread.request_stop();
    for (auto& thread : meshBuildThreads) thread.request_stop();

    // Wake up any waiting threads so they can see the stop request
    generationQueueCV.notify_all();
    meshBuildQueueCV.notify_all();

    generationThreads.clear();
    meshBuildThreads.clear();

    // Clear queues to avoid dangling Chunk* pointers
    {
        std::lock_guard lock(generationQueueMutex);
        generationQueue = {};
    }
    {
        std::lock_guard lock(meshBuildQueueMutex);
        meshBuildQueue = {};
    }
    {
        std::lock_guard lock(gpuUploadQueueMutex);
        gpuUploadQueue = {};
    }
}

bool World::processOneGenerationTask() {
    ChunkGenerationTask task;
    {
        std::lock_guard<std::mutex> lock(generationQueueMutex);
        if (generationQueue.empty()) {
            return false;
        }
        task = generationQueue.front();
        generationQueue.pop();
    }

    // A task for a chunk that has since been unloaded still counts as consumed, so the
    // caller keeps draining rather than treating it as an empty queue.
    if (isChunkLoaded(task.chunkPos)) {
        task.chunk->setState(ChunkState::Generating);
        task.chunk->generate(&perlinNoise, worleyBiome.get());

        {
            std::lock_guard<std::mutex> lock(pendingEntitySpawnsMutex);
            pendingEntitySpawns.push(task.chunkPos);
        }

        {
            std::lock_guard<std::mutex> lock(meshBuildQueueMutex);
            meshBuildQueue.push({task.chunk, MeshData(), MeshData()});
        }
        meshBuildQueueCV.notify_one();
    }

    return true;
}

void World::generationWorkerThread(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        {
            std::unique_lock<std::mutex> lock(generationQueueMutex);
            generationQueueCV.wait(lock, stopToken, [this] {
                return !generationQueue.empty();
            });
        }

        // shutdownThreadPool() clears the queues right after requesting a stop, so there
        // is nothing left worth draining here.
        if (stopToken.stop_requested()) {
            return;
        }

        // May lose the race to another worker, in which case this is a no-op and the
        // loop goes back to waiting.
        processOneGenerationTask();
    }
}

void World::processPendingEntitySpawns(int maxPerFrame) {
    int processed = 0;

    // TODO: Maybe make entities spawn in group (e.g. family of cows)
    while (processed < maxPerFrame) {
        glm::ivec3 pos;

        {
            std::lock_guard<std::mutex> lock(pendingEntitySpawnsMutex);
            if (pendingEntitySpawns.empty()) break;
            pos = pendingEntitySpawns.front();
            pendingEntitySpawns.pop();
        }

        entityManager.spawnAnimalsInChunk(pos, this);
        processed++;
    }
}

bool World::processOneMeshBuildTask() {
    ChunkMeshTask task;
    {
        std::lock_guard<std::mutex> lock(meshBuildQueueMutex);
        if (meshBuildQueue.empty()) {
            return false;
        }
        task = meshBuildQueue.front();
        meshBuildQueue.pop();
    }

    const glm::ivec3 chunkPos = task.chunk->getPosition();
    if (!isChunkLoaded(chunkPos)) {
        return true;
    }

    const ChunkState currentState = task.chunk->getState();
    if (currentState == ChunkState::Generated || currentState == ChunkState::BuildingMesh) {
        task.chunk->setState(ChunkState::BuildingMesh);
        task.chunk->buildMeshData(task.meshData, &textureAtlas, this);
        task.chunk->buildTransparentMeshData(task.transparentMeshData, &textureAtlas);

        // Re-checked because the chunk can be unloaded while the mesh is being built.
        if (isChunkLoaded(chunkPos)) {
            task.chunk->setState(ChunkState::MeshBuilt);
            {
                std::lock_guard<std::mutex> lock(gpuUploadQueueMutex);
                gpuUploadQueue.push(std::move(task));
            }
        }
    }

    return true;
}

void World::meshBuildWorkerThread(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        {
            std::unique_lock<std::mutex> lock(meshBuildQueueMutex);
            meshBuildQueueCV.wait(lock, stopToken, [this] {
                return !meshBuildQueue.empty();
            });
        }

        if (stopToken.stop_requested()) {
            return;
        }

        processOneMeshBuildTask();
    }
}

void World::pumpChunkWork(std::chrono::microseconds budget) {
    PROFILE_SCOPE("World::pumpChunkWork");

    const auto deadline = std::chrono::steady_clock::now() + budget;

    while (true) {
        // Meshing first: generation feeds it, and meshing is what turns a generated
        // chunk into something the renderer can actually draw.
        if (!processOneMeshBuildTask() && !processOneGenerationTask()) {
            return;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            return;
        }
    }
}

void World::processGPUUploadQueue(int maxPerFrame) {
    PROFILE_SCOPE("World::processGPUUploadQueue");

    std::lock_guard<std::mutex> lock(gpuUploadQueueMutex);

    int processed = 0;
    while (!gpuUploadQueue.empty() && processed < maxPerFrame) {
        ChunkMeshTask& task = gpuUploadQueue.front();

        glm::ivec3 chunkPos = task.chunk->getPosition();

        if (isChunkLoaded(chunkPos) && task.chunk->getState() == ChunkState::MeshBuilt) {
            task.chunk->uploadMeshToGPU(task.meshData);
            task.chunk->uploadTransparentMeshToGPU(task.transparentMeshData);
            // TODO: Implement proper neighbor remeshing with cycle detection
        }

        gpuUploadQueue.pop();
        processed++;
    }
}

void World::update(const glm::vec3& cameraPosition) {
    PROFILE_FUNCTION();

    {
        PROFILE_SCOPE("World::update - chunk loading");
        glm::ivec3 currentChunkPos = worldToChunkPos(
            static_cast<int>(std::floor(cameraPosition.x)),
            static_cast<int>(std::floor(cameraPosition.y)),
            static_cast<int>(std::floor(cameraPosition.z))
        );

        if (currentChunkPos != lastCameraChunkPos) {
            loadChunksAroundPosition(currentChunkPos);
            unloadDistantChunks(currentChunkPos);
            lastCameraChunkPos = currentChunkPos;
        }
    }

#ifdef GLFWVOXEL_SINGLE_THREADED
    pumpChunkWork(CHUNK_WORK_BUDGET);
#endif

    {
        PROFILE_SCOPE("World::update - GPU uploads");
        processGPUUploadQueue(2);
    }

    {
        PROFILE_SCOPE("World::update - entity spawns");
        processPendingEntitySpawns(4);
    }
}

BlockType World::getBlock(int worldX, int worldY, int worldZ) {
    glm::ivec3 chunkPos = worldToChunkPos(worldX, worldY, worldZ);
    Chunk* chunk = getChunk(chunkPos);
    if (!chunk)
        return BlockType::Air;

    glm::ivec3 localPos = worldToLocalPos(worldX, worldY, worldZ);
    return chunk->getBlock(localPos.x, localPos.y, localPos.z);
}

void World::setBlock(int worldX, int worldY, int worldZ, BlockType type) {
    glm::ivec3 chunkPos = worldToChunkPos(worldX, worldY, worldZ);

    Chunk* chunk = getChunk(chunkPos);
    if (!chunk) return;

    // Only check occupation when placing blocks
    if (type != BlockType::Air) {
        AABB blockBox{
            glm::vec3(worldX, worldY, worldZ),
            glm::vec3(worldX + 1, worldY + 1, worldZ + 1)
        };

        if (player.getBoundingBox().intersects(blockBox)) {
            return;
        }

        std::vector<Entity*> entitiesInChunk = entityManager.getEntitiesInChunk(chunkPos);
        for (Entity* entity : entitiesInChunk) {
            if (entity->getBoundingBox().intersects(blockBox)) {
                return;
            }
        }
    }

    glm::ivec3 localPos = worldToLocalPos(worldX, worldY, worldZ);
    const BlockType previous = chunk->getBlock(localPos.x, localPos.y, localPos.z);
    chunk->setBlock(localPos.x, localPos.y, localPos.z, type);

    if (onBlockChange && previous != type) {
        onBlockChange(glm::ivec3(worldX, worldY, worldZ), previous, type);
    }

    if (chunk->getState() == ChunkState::Ready) {
        chunk->markDirty();
        chunk->setState(ChunkState::Generated);
        std::lock_guard<std::mutex> lock(meshBuildQueueMutex);
        meshBuildQueue.push({chunk, MeshData(), MeshData()});
        meshBuildQueueCV.notify_one();
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

bool World::hasTerrainAt(int worldX, int worldZ) {
    const Chunk* chunk = getChunk(worldToChunkPos(worldX, 0, worldZ));
    return chunk != nullptr && chunk->getState() >= ChunkState::Generated;
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
    std::vector<ChunkGenerationTask> toLoad;

    for (int x = -renderDistance; x <= renderDistance; x++) {
        for (int z = -renderDistance; z <= renderDistance; z++) {
            if (std::sqrt(x * x + z * z) > renderDistance) continue;

            glm::ivec3 chunkPos = centerChunkPos + glm::ivec3(x, 0, z);

            if (!isChunkLoaded(chunkPos)) {
                auto chunk = std::make_unique<Chunk>(chunkPos);
                Chunk* chunkPtr = chunk.get();
                m_chunks[chunkPos] = std::move(chunk);
                toLoad.push_back({chunkPos, chunkPtr});
            }
        }
    }

    // Sort closest to player first
    std::sort(toLoad.begin(), toLoad.end(), [&](const ChunkGenerationTask& a, const ChunkGenerationTask& b) {
        auto distSq = [&](const glm::ivec3& pos) {
            glm::ivec3 d = pos - centerChunkPos;
            return d.x * d.x + d.z * d.z;
        };
        return distSq(a.chunkPos) < distSq(b.chunkPos);
    });

    {
        std::lock_guard<std::mutex> lock(generationQueueMutex);
        for (auto& task : toLoad) {
            generationQueue.push(task);
        }
    }
    generationQueueCV.notify_all();
}

void World::unloadDistantChunks(const glm::ivec3& centerChunkPos) {
    std::vector<glm::ivec3> chunksToUnload;

    for (auto& pair : m_chunks) {
        const glm::ivec3& chunkPos = pair.first;

        // Calculate distance from center
        glm::ivec3 diff = chunkPos - centerChunkPos;
        float distance = std::sqrt(diff.x * diff.x + diff.z * diff.z);

        if (distance > renderDistance + 2) {
            Chunk* chunk = pair.second.get();
            ChunkState state = chunk->getState();

            // Only unload chunks that are ready or empty
            if (state == ChunkState::Ready || state == ChunkState::Empty) {
                chunksToUnload.push_back(chunkPos);
            }
        }
    }

    for (const glm::ivec3& pos : chunksToUnload) {
        m_chunks.erase(pos);
    }
}

bool World::isChunkLoaded(const glm::ivec3& chunkPos) const {
    return m_chunks.find(chunkPos) != m_chunks.end();
}

const Biome* World::getCurrentPlayerBiome(float cameraX, float cameraZ) const {
    return worleyBiome->getBiomeAt(static_cast<int>(cameraX), static_cast<int>(cameraZ));
}

RaycastResult World::raycastBlock(const glm::vec3& origin, const glm::vec3& direction, float maxDistance) {
    RaycastResult result{false, glm::vec3(0.0f), glm::vec3(0.0f)};

    const glm::vec3 dir = glm::normalize(direction);
    glm::ivec3 cell(static_cast<int>(std::floor(origin.x)),
                    static_cast<int>(std::floor(origin.y)),
                    static_cast<int>(std::floor(origin.z)));

    // Per axis: which way we walk, how far to the first boundary, and how far between boundaries once marching.
    glm::ivec3 stepDir(0);
    glm::vec3 tMax(0.0f);
    glm::vec3 tDelta(0.0f);

    constexpr float NEVER = std::numeric_limits<float>::max();

    for (int axis = 0; axis < 3; ++axis) {
        if (dir[axis] > 0.0f) {
            stepDir[axis] = 1;
            tMax[axis] = (static_cast<float>(cell[axis] + 1) - origin[axis]) / dir[axis];
        } else if (dir[axis] < 0.0f) {
            stepDir[axis] = -1;
            tMax[axis] = (origin[axis] - static_cast<float>(cell[axis])) / -dir[axis];
        } else {
            // Parallel to this axis, so its boundaries are never reached.
            stepDir[axis] = 0;
            tMax[axis] = NEVER;
        }
        tDelta[axis] = dir[axis] != 0.0f ? std::abs(1.0f / dir[axis]) : NEVER;
    }

    float travelled = 0.0f;
    while (travelled < maxDistance) {
        // Cross whichever boundary comes first. That axis is the face we enter through.
        const int axis = (tMax.x < tMax.y) ? ((tMax.x < tMax.z) ? 0 : 2)
                                           : ((tMax.y < tMax.z) ? 1 : 2);

        travelled = tMax[axis];
        if (travelled >= maxDistance) {
            break;
        }

        tMax[axis] += tDelta[axis];
        cell[axis] += stepDir[axis];

        if (getBlock(cell.x, cell.y, cell.z) != BlockType::Air) {
            result.hit = true;
            result.hitPos = cell;
            result.hitNormal = glm::vec3(0.0f);
            result.hitNormal[axis] = static_cast<float>(stepDir[axis]);
            return result;
        }
    }

    return result;
}
