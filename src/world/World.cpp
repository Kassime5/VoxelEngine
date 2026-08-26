//
// Created by maxim on 03/01/2026.
//

#include "World.h"
#include "src/debug/TestScene.h"
#include "src/game/Player.h"

#include <limits>
#include <random>
#include <stdexcept>
#include <utility>

namespace {
#ifdef GLFWVOXEL_SINGLE_THREADED
    // to avoid hangs on single threads
    constexpr std::chrono::microseconds CHUNK_WORK_BUDGET{6000};
#endif

    constexpr std::chrono::microseconds GPU_UPLOAD_BUDGET{2000};
    constexpr int GPU_UPLOAD_MIN_PER_FRAME = 2;
    constexpr int GPU_UPLOAD_MAX_ATTEMPTS = 64;
}

World::SeedType World::randomSeed() {
    std::random_device rd;
    return static_cast<SeedType>(rd());
}

World::SeedType World::seedFromString(const std::string& text) {
    const std::size_t first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return randomSeed();
    }
    const std::string trimmed = text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);

    if (trimmed.find_first_not_of("0123456789") == std::string::npos) {
        try {
            // Wider than SeedType on purpose: a long digit string truncates rather than throws
            return static_cast<SeedType>(std::stoull(trimmed));
        } catch (const std::out_of_range&) {
            // too long even for that, fall through and hash it
        }
    }

    // FNV-1a
    SeedType hash = 2166136261u;
    for (const char c : trimmed) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 16777619u;
    }
    return hash;
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

    {
        std::unique_lock lock(chunksMutex);
        m_chunks.clear();
    }
    chunkGraveyard.clear();
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
        editMeshBuildQueue = {};
    }
    {
        std::lock_guard lock(gpuUploadQueueMutex);
        gpuUploadQueue = {};
        editGpuUploadQueue = {};
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
            meshBuildQueue.push({task.chunk, MeshData(), MeshData(), MeshData()});
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
    if (TestScene::enabled) return;

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
        std::queue<ChunkMeshTask>& queue = editMeshBuildQueue.empty() ? meshBuildQueue : editMeshBuildQueue;
        if (queue.empty()) {
            return false;
        }
        task = std::move(queue.front());
        queue.pop();
    }

    const glm::ivec3 chunkPos = task.chunk->getPosition();
    if (!isChunkLoaded(chunkPos)) {
        return true;
    }

    const ChunkState currentState = task.chunk->getState();
    if (currentState == ChunkState::Generated || currentState == ChunkState::BuildingMesh) {
        task.chunk->setState(ChunkState::BuildingMesh);

        // Read before the build, so an edit landing mid-build leaves the two out of step.
        task.editVersion = task.chunk->getEditVersion();

        const ChunkNeighbourhood neighbours = snapshotNeighbourhood(chunkPos);
        task.chunk->buildMeshData(task.meshData, &textureAtlas, neighbours);
        task.chunk->buildTransparentMeshData(task.transparentMeshData, &textureAtlas);
        task.chunk->buildWaterMeshData(task.waterMeshData, &textureAtlas, neighbours);

        // Re-checked because the chunk can be unloaded while the mesh is being built.
        if (isChunkLoaded(chunkPos)) {
            task.chunk->setState(ChunkState::MeshBuilt);
            {
                std::lock_guard<std::mutex> lock(gpuUploadQueueMutex);
                std::queue<ChunkMeshTask>& queue = task.playerEdit ? editGpuUploadQueue : gpuUploadQueue;
                queue.push(std::move(task));
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
                return !meshBuildQueue.empty() || !editMeshBuildQueue.empty();
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

std::shared_ptr<Chunk> World::getChunkShared(const glm::ivec3& chunkPos) const {
    std::shared_lock lock(chunksMutex);
    auto it = m_chunks.find(chunkPos);
    return it != m_chunks.end() ? it->second : nullptr;
}

// Takes the chunk by value: the queued task has to keep it alive until a worker gets to it.
void World::queueRemesh(std::shared_ptr<Chunk> chunk, bool playerEdit) {
    // Anything not yet Ready already has a build queued or running. That build either reads
    // the edit outright or finishes carrying a stale editVersion, and the upload re-queues it.
    if (!chunk || chunk->getState() != ChunkState::Ready) {
        return;
    }

    chunk->setState(ChunkState::Generated);
    {
        ChunkMeshTask task;
        task.chunk = std::move(chunk);
        task.playerEdit = playerEdit;

        std::lock_guard<std::mutex> lock(meshBuildQueueMutex);
        std::queue<ChunkMeshTask>& queue = playerEdit ? editMeshBuildQueue : meshBuildQueue;
        queue.push(std::move(task));
    }
    meshBuildQueueCV.notify_one();
}

void World::collectStaleNeighbours(const glm::ivec3& chunkPos,
                                   std::vector<std::shared_ptr<Chunk>>& out) {
    // Chunks span the full world height, so only the four horizontal neighbours share a seam.
    const glm::ivec3 offsets[4] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1}
    };

    for (const glm::ivec3& offset : offsets) {
        if (auto neighbour = getChunkShared(chunkPos + offset)) {
            out.push_back(std::move(neighbour));
        }
    }
}

void World::processGPUUploadQueue(std::chrono::microseconds budget) {
    PROFILE_SCOPE("World::processGPUUploadQueue");

    const auto deadline = std::chrono::steady_clock::now() + budget;

    std::vector<std::shared_ptr<Chunk>> staleNeighbours;

    // Chunk plus whether its edit came from the player, so the remesh keeps its priority.
    std::vector<std::pair<std::shared_ptr<Chunk>, bool>> outdated;
    int uploaded = 0;

    for (int attempt = 0; attempt < GPU_UPLOAD_MAX_ATTEMPTS; attempt++) {
        ChunkMeshTask task;
        {
            // Popped one at a time so the GL uploads below never run under this lock
            std::lock_guard<std::mutex> lock(gpuUploadQueueMutex);
            std::queue<ChunkMeshTask>& queue = editGpuUploadQueue.empty() ? gpuUploadQueue
                                                                         : editGpuUploadQueue;
            if (queue.empty()) {
                break;
            }
            task = std::move(queue.front());
            queue.pop();
        }

        const glm::ivec3 chunkPos = task.chunk->getPosition();

        if (!isChunkLoaded(chunkPos) || task.chunk->getState() != ChunkState::MeshBuilt) {
            continue;
        }

        task.chunk->uploadMeshToGPU(task.meshData);
        task.chunk->uploadTransparentMeshToGPU(task.transparentMeshData);
        task.chunk->uploadWaterMeshToGPU(task.waterMeshData);
        uploaded++;

        // Only on a chunk's first arrival. Remeshing never re-notifies, which is what
        // makes the cascade terminate instead of ringing between neighbours.
        if (!task.chunk->hasNotifiedNeighbours()) {
            task.chunk->markNeighboursNotified();
            collectStaleNeighbours(chunkPos, staleNeighbours);
        }

        // Blocks were edited after this mesh was built, requeue it
        if (task.chunk->getEditVersion() != task.editVersion) {
            outdated.emplace_back(std::move(task.chunk), task.playerEdit);
        }

        if (uploaded >= GPU_UPLOAD_MIN_PER_FRAME && std::chrono::steady_clock::now() >= deadline) {
            break;
        }
    }

    for (auto& [chunk, playerEdit] : outdated) {
        queueRemesh(std::move(chunk), playerEdit);
    }

    for (auto& neighbour : staleNeighbours) {
        queueRemesh(std::move(neighbour), false);
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
        processGPUUploadQueue(GPU_UPLOAD_BUDGET);
    }

    // Main thread, so the GL deletes in ~Chunk have a context.
    reapUnloadedChunks();

    {
        PROFILE_SCOPE("World::update - entity spawns");
        processPendingEntitySpawns(4);
    }
}

BlockType World::getBlock(int worldX, int worldY, int worldZ) {
    const glm::ivec3 chunkPos = worldToChunkPos(worldX, worldY, worldZ);

    // The shared_ptr outlives the lock, so the chunk cannot be unloaded out from under
    // the read that follows.
    std::shared_ptr<Chunk> chunk;
    {
        std::shared_lock lock(chunksMutex);
        auto it = m_chunks.find(chunkPos);
        if (it == m_chunks.end())
            return BlockType::Air;
        chunk = it->second;
    }

    glm::ivec3 localPos = worldToLocalPos(worldX, worldY, worldZ);
    return chunk->getBlock(localPos.x, localPos.y, localPos.z);
}

void World::setBlock(int worldX, int worldY, int worldZ, BlockType type) {
    glm::ivec3 chunkPos = worldToChunkPos(worldX, worldY, worldZ);

    std::shared_ptr<Chunk> chunk = getChunkShared(chunkPos);
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

    queueRemesh(chunk, true);

    // An edit on a seam changes what the neighbour should draw there, so its mesh is stale
    // too. A corner block touches two of them.
    if (localPos.x == 0) {
        queueRemesh(getChunkShared(chunkPos + glm::ivec3(-1, 0, 0)), true);
    } else if (localPos.x == Chunk::SIZE - 1) {
        queueRemesh(getChunkShared(chunkPos + glm::ivec3(1, 0, 0)), true);
    }
    if (localPos.z == 0) {
        queueRemesh(getChunkShared(chunkPos + glm::ivec3(0, 0, -1)), true);
    } else if (localPos.z == Chunk::SIZE - 1) {
        queueRemesh(getChunkShared(chunkPos + glm::ivec3(0, 0, 1)), true);
    }
}
Chunk* World::getChunk(const glm::ivec3& chunkPos) {
    std::shared_lock lock(chunksMutex);
    auto it = m_chunks.find(chunkPos);
    return (it != m_chunks.end()) ? it->second.get() : nullptr;
}

ChunkNeighbourhood World::snapshotNeighbourhood(const glm::ivec3& chunkPos) const {
    ChunkNeighbourhood snapshot;
    snapshot.centre = chunkPos;

    // One lock for the whole snapshot rather than one per boundary voxel, which is what
    // meshing would otherwise cost -- roughly a hundred thousand lookups per chunk.
    std::shared_lock lock(chunksMutex);

    auto pin = [this](const glm::ivec3& pos) -> std::shared_ptr<Chunk> {
        auto it = m_chunks.find(pos);
        if (it == m_chunks.end() || it->second->getState() < ChunkState::Generated) {
            return nullptr;
        }
        return it->second;
    };

    snapshot.negX = pin(chunkPos + glm::ivec3(-1, 0, 0));
    snapshot.posX = pin(chunkPos + glm::ivec3(1, 0, 0));
    snapshot.negZ = pin(chunkPos + glm::ivec3(0, 0, -1));
    snapshot.posZ = pin(chunkPos + glm::ivec3(0, 0, 1));
    return snapshot;
}

Chunk* World::getChunkAt(int worldX, int worldY, int worldZ) {
    glm::ivec3 chunkPos = worldToChunkPos(worldX, worldY, worldZ);
    return getChunk(chunkPos);
}

bool World::hasTerrainAt(int worldX, int worldZ) {
    const glm::ivec3 chunkPos = worldToChunkPos(worldX, 0, worldZ);
    std::shared_lock lock(chunksMutex);
    auto it = m_chunks.find(chunkPos);
    return it != m_chunks.end() && it->second->getState() >= ChunkState::Generated;
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

            std::unique_lock lock(chunksMutex);
            if (m_chunks.find(chunkPos) == m_chunks.end()) {
                auto chunk = std::make_shared<Chunk>(chunkPos);
                m_chunks[chunkPos] = chunk;
                toLoad.push_back({chunkPos, std::move(chunk)});
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

    std::unique_lock lock(chunksMutex);

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

    // Parked rather than dropped. A Chunk owns GL buffers and its destructor deletes them,
    // so it must die on the thread holding the context -- but a worker can be the last one
    // holding it (a neighbour snapshot pins chunks that then drift out of range). Keeping a
    // reference here guarantees the final release happens in reapUnloadedChunks instead.
    for (const glm::ivec3& pos : chunksToUnload) {
        auto it = m_chunks.find(pos);
        if (it != m_chunks.end()) {
            chunkGraveyard.push_back(std::move(it->second));
            m_chunks.erase(it);
        }
    }
}

void World::reapUnloadedChunks() {
    // use_count() == 1 means only the graveyard still refers to it, and nothing can take a
    // new reference now that it is out of the map. Anything else is still in a worker's
    // hands, so it waits for a later frame.
    std::erase_if(chunkGraveyard, [](const std::shared_ptr<Chunk>& chunk) {
        return chunk.use_count() == 1;
    });
}

bool World::isChunkLoaded(const glm::ivec3& chunkPos) const {
    std::shared_lock lock(chunksMutex);
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

        if (!canRaycastThrough(getBlock(cell.x, cell.y, cell.z))) {
            result.hit = true;
            result.hitPos = cell;
            result.hitNormal = glm::vec3(0.0f);
            result.hitNormal[axis] = static_cast<float>(stepDir[axis]);
            return result;
        }
    }

    return result;
}
