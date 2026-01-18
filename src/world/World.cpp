//
// Created by maxim on 03/01/2026.
//

#include "World.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include "src/debug/RenderStats.h"

World::World()
    : renderDistance(8), lastCameraChunkPos(INT_MAX, INT_MAX, INT_MAX),
      seed(1010), perlinNoise(seed), stopThreads(false) {
    worleyGenerator = new WorleyBiome(seed, 128);
    initThreadPool(4, 4);
}

World::~World() {
    shutdownThreadPool();
    delete worleyGenerator;
}

void World::initThreadPool(int _generationThreads, int _meshThreads) {
    stopThreads = false;
    for (int i = 0; i < _generationThreads; i++) {
        generationThreads.emplace_back(&World::generationWorkerThread, this);
    }
    for (int i = 0; i < _meshThreads; i++) {
        meshBuildThreads.emplace_back(&World::meshBuildWorkerThread, this);
    }
}

void World::shutdownThreadPool() {
    stopThreads = true;
    generationQueueCV.notify_all();
    meshBuildQueueCV.notify_all();

    for (auto& thread : generationThreads) {
        if (thread.joinable()) thread.join();
    }
    for (auto& thread : meshBuildThreads) {
        if (thread.joinable()) thread.join();
    }

    generationThreads.clear();
    meshBuildThreads.clear();
}

void World::generationWorkerThread() {
    while (!stopThreads) {
        ChunkGenerationTask task;

        {
            std::unique_lock<std::mutex> lock(generationQueueMutex);
            generationQueueCV.wait(lock, [this] {
                return stopThreads || !generationQueue.empty();
            });

            if (stopThreads && generationQueue.empty()) return;

            if (!generationQueue.empty()) {
                task = generationQueue.front();
                generationQueue.pop();
            } else {
                continue;
            }
        }

        if (isChunkLoaded(task.chunkPos)) {
            task.chunk->setState(ChunkState::Generating);
            task.chunk->generate(&perlinNoise, worleyGenerator);

            if (isChunkLoaded(task.chunkPos)) {
                std::lock_guard<std::mutex> lock(meshBuildQueueMutex);
                meshBuildQueue.push({task.chunk, MeshData()});
                meshBuildQueueCV.notify_one();
            }
        }
    }
}

void World::meshBuildWorkerThread() {
    while (!stopThreads) {
        ChunkMeshTask task;

        {
            std::unique_lock<std::mutex> lock(meshBuildQueueMutex);
            meshBuildQueueCV.wait(lock, [this] {
                return stopThreads || !meshBuildQueue.empty();
            });

            if (stopThreads && meshBuildQueue.empty()) return;

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
}

void World::render(Shader& shader) {
    textureAtlas.bind(0);

    for (auto& pair : m_chunks) {
        Chunk* chunk = pair.second.get();

        if (chunk->getState() != ChunkState::Ready) {
            RenderStats::getInstance().addChunkSkipped();
            continue;
        }

        glm::mat4 model = glm::mat4(1.0f);
        glm::vec3 worldPos = glm::vec3(pair.first) * static_cast<float>(Chunk::SIZE);
        model = glm::translate(model, worldPos);
        shader.setMat4("model", model);

        chunk->draw();
        RenderStats::getInstance().addChunkRendered();
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

    glm::ivec3 localPos = worldToLocalPos(worldX, worldY, worldZ);
    chunk->setBlock(localPos.x, localPos.y, localPos.z, type);

    if (chunk->getState() == ChunkState::Ready) {
        chunk->markDirty();
        chunk->setState(ChunkState::Generated);
        std::lock_guard<std::mutex> lock(meshBuildQueueMutex);
        meshBuildQueue.push({chunk, MeshData()});
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
    for (int x = -renderDistance; x <= renderDistance; x++) {
        for (int z = -renderDistance; z <= renderDistance; z++) {
            float distance = std::sqrt(x * x + z * z);
            if (distance > renderDistance) continue;

            glm::ivec3 chunkPos = centerChunkPos + glm::ivec3(x, 0, z);

            if (!isChunkLoaded(chunkPos)) {
                auto chunk = std::make_unique<Chunk>(chunkPos);
                Chunk* chunkPtr = chunk.get();
                m_chunks[chunkPos] = std::move(chunk);

                {
                    std::lock_guard<std::mutex> lock(generationQueueMutex);
                    generationQueue.push({chunkPos, chunkPtr});
                }
                generationQueueCV.notify_one();
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

void World::printDebugInfo() const {
    int empty = 0, generating = 0, generated = 0, buildingMesh = 0, meshBuilt = 0, ready = 0;
    for (const auto& pair : m_chunks) {
        switch (pair.second->getState()) {
            case ChunkState::Empty: empty++; break;
            case ChunkState::Generating: generating++; break;
            case ChunkState::Generated: generated++; break;
            case ChunkState::BuildingMesh: buildingMesh++; break;
            case ChunkState::MeshBuilt: meshBuilt++; break;
            case ChunkState::Ready: ready++; break;
        }
    }
    std::cout << "\n=== Chunk Status ===\n";
    std::cout << "Total: " << m_chunks.size() << "\n";
    std::cout << "Empty: " << empty << " | Generating: " << generating << "\n";
    std::cout << "Generated: " << generated << " | BuildingMesh: " << buildingMesh << "\n";
    std::cout << "MeshBuilt: " << meshBuilt << " | Ready: " << ready << "\n";
    std::cout << "===================\n";
}