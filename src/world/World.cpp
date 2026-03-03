//
// Created by maxim on 03/01/2026.
//

#include "World.h"
#include "src/game/Player.h"

World::World(Player& _player)
    : player(_player), renderDistance(8), lastCameraChunkPos(INT_MAX, INT_MAX, INT_MAX),
      seed(12345), perlinNoise(seed)
{
    worleyBiome = std::make_unique<WorleyBiome>(seed, 384);
    initThreadPool(6, 6);

    ShaderManager& sm = ShaderManager::getInstance();
    sm.addShader("terrain", "assets/shader/terrain/terrain.vs.glsl",
                 "assets/shader/terrain/terrain.fs.glsl");
    terrainShader = sm.getShader("terrain");

    if (!loadTextureAtlas("assets/textures/atlas2.png", 8))
    {
        std::cerr << "Failed to load texture atlas!" << std::endl;
    }
}

World::~World() {
    shutdownThreadPool();
}

void World::initThreadPool(int _generationThreads, int _meshThreads) {
    for (int i = 0; i < _generationThreads; i++) {
        generationThreads.emplace_back(&World::generationWorkerThread, this);
    }
    for (int i = 0; i < _meshThreads; i++) {
        meshBuildThreads.emplace_back(&World::meshBuildWorkerThread, this);
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

void World::generationWorkerThread(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        ChunkGenerationTask task;
        {
            std::unique_lock<std::mutex> lock(generationQueueMutex);
            generationQueueCV.wait(lock, stopToken, [this] {
                return !generationQueue.empty();
            });

            if (stopToken.stop_requested() && generationQueue.empty()) return;

            if (!generationQueue.empty()) {
                task = generationQueue.front();
                generationQueue.pop();
            } else {
                continue;
            }
        }

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

void World::meshBuildWorkerThread(std::stop_token stopToken) {
    while (!stopToken.stop_requested()) {
        ChunkMeshTask task;

        {
            std::unique_lock<std::mutex> lock(meshBuildQueueMutex);
            meshBuildQueueCV.wait(lock, stopToken, [this] {
                return !meshBuildQueue.empty();
            });

            if (stopToken.stop_requested() && meshBuildQueue.empty()) return;

            if (!meshBuildQueue.empty()) {
                task = meshBuildQueue.front();
                meshBuildQueue.pop();
            } else {
                continue;
            }
        }

        glm::ivec3 chunkPos = task.chunk->getPosition();

        if (!isChunkLoaded(chunkPos)) {
            continue;
        }

        ChunkState currentState = task.chunk->getState();
        if (currentState == ChunkState::Generated || currentState == ChunkState::BuildingMesh) {
            task.chunk->setState(ChunkState::BuildingMesh);
            task.chunk->buildMeshData(task.meshData, &textureAtlas, this);
            task.chunk->buildTransparentMeshData(task.transparentMeshData, &textureAtlas);

            if (isChunkLoaded(chunkPos)) {
                task.chunk->setState(ChunkState::MeshBuilt);
                {
                    std::lock_guard<std::mutex> lock(gpuUploadQueueMutex);
                    gpuUploadQueue.push(std::move(task));
                }
            }
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

bool World::loadTextureAtlas(const char* atlasPath, int tilesPerRow) {
    return textureAtlas.load(atlasPath, tilesPerRow);
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

    {
        PROFILE_SCOPE("World::update - GPU uploads");
        processGPUUploadQueue(2);
    }

    {
        PROFILE_SCOPE("World::update - entity spawns");
        processPendingEntitySpawns(4);
    }
}

void World::renderWorld(glm::mat4 projection, glm::mat4 view) {
    terrainShader->use();
    terrainShader->setFloat("tilesPerRow", 8.0f);
    terrainShader->setInt("texture1", 0);

    // TODO: Change where the lightsource is
    terrainShader->setVec3("lightPos", glm::vec3(0.0f, 50.0f, 0.0f));
    terrainShader->setVec3("lightColor", glm::vec3(1.0f, 1.0f, 1.0f));
    terrainShader->setMat4("projection", projection);
    terrainShader->setMat4("view", view);

    render();
    renderTransparent();

    entityManager.render(projection, view);
    entityManager.renderDebug(projection, view);
}

void World::render() {
    glm::mat4 viewProj = player.getCamera().GetProjectionMatrix() * player.getCamera().GetViewMatrix();

    Frustum frustum;
    frustum.extractFromMatrix(viewProj);

    textureAtlas.bind(0);
    constexpr float chunkSize = static_cast<float>(Chunk::SIZE);
    constexpr float chunkHeight = static_cast<float>(Chunk::HEIGHT);

    for (const auto& [chunkPos, chunk] : m_chunks) {
        glm::vec3 worldPos(chunkPos.x * chunkSize, chunkPos.y, chunkPos.z * chunkSize);
        glm::vec3 chunkMin = worldPos;
        glm::vec3 chunkMax = worldPos + glm::vec3(chunkSize, chunkHeight, chunkSize);

        if (!frustum.isBoxInFrustum(chunkMin, chunkMax)) {
            RenderStats::getInstance().addChunkCulled();
            continue;
        }

        terrainShader->setVec3("chunkOffset", worldPos);
        RenderStats::getInstance().addChunkRendered();
        chunk->draw();
    }
}

void World::renderTransparent() {
    glm::mat4 viewProj = player.getCamera().GetProjectionMatrix() * player.getCamera().GetViewMatrix();
    Frustum frustum;
    frustum.extractFromMatrix(viewProj);

    // textureAtlas.bind(0);
    constexpr float chunkSize = static_cast<float>(Chunk::SIZE);
    constexpr float chunkHeight = static_cast<float>(Chunk::HEIGHT);

    glDisable(GL_CULL_FACE);
    glDepthMask(GL_FALSE);

    for (const auto& [chunkPos, chunk] : m_chunks) {
        if (chunk->isTransparentMeshEmpty())
            continue;

        glm::vec3 worldPos(chunkPos.x * chunkSize, chunkPos.y, chunkPos.z * chunkSize);
        glm::vec3 chunkMin = worldPos;
        glm::vec3 chunkMax = worldPos + glm::vec3(chunkSize, chunkHeight, chunkSize);

        if (!frustum.isBoxInFrustum(chunkMin, chunkMax)) {
            continue;
        }

        terrainShader->setVec3("chunkOffset", worldPos);
        chunk->drawTransparent();
    }

    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
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

    glm::ivec3 localPos = worldToLocalPos(worldX, worldY, worldZ);
    chunk->setBlock(localPos.x, localPos.y, localPos.z, type);

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
    RaycastResult result{false, glm::ivec3(0), glm::ivec3(0)};

    glm::vec3 pos = origin;
    glm::vec3 step = glm::normalize(direction) * 0.1f;

    float distance = 0.0f;
    glm::ivec3 lastPos = glm::floor(pos);

    while (distance < maxDistance) {
        pos += step;
        distance += 0.1f;

        glm::ivec3 blockPos(std::floor(pos.x), std::floor(pos.y), std::floor(pos.z));

        if (blockPos != lastPos) {
            BlockType block = getBlock(blockPos.x, blockPos.y, blockPos.z);

            if (block != BlockType::Air) {
                result.hit = true;
                result.hitPos = blockPos;
                result.hitNormal = blockPos - lastPos;
                return result;
            }

            lastPos = blockPos;
        }
    }

    return result;
}
