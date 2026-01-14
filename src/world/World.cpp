//
// Created by maxim on 03/01/2026.
//

#include "World.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

World::World()
    : m_renderDistance(8), m_lastCameraChunkPos(INT_MAX, INT_MAX, INT_MAX),
      seed(1010), perlinNoise(seed), m_stopThreads(false) {
    initThreadPool(4, 4);
}

World::~World() {
    shutdownThreadPool();
}

void World::initThreadPool(int generationThreads, int meshThreads) {
    m_stopThreads = false;
    for (int i = 0; i < generationThreads; i++) {
        m_generationThreads.emplace_back(&World::generationWorkerThread, this);
    }
    for (int i = 0; i < meshThreads; i++) {
        m_meshBuildThreads.emplace_back(&World::meshBuildWorkerThread, this);
    }
}

void World::shutdownThreadPool() {
    m_stopThreads = true;
    m_generationQueueCV.notify_all();
    m_meshBuildQueueCV.notify_all();

    for (auto& thread : m_generationThreads) {
        if (thread.joinable()) thread.join();
    }
    for (auto& thread : m_meshBuildThreads) {
        if (thread.joinable()) thread.join();
    }

    m_generationThreads.clear();
    m_meshBuildThreads.clear();
}

void World::generationWorkerThread() {
    while (!m_stopThreads) {
        ChunkGenerationTask task;

        {
            std::unique_lock<std::mutex> lock(m_generationQueueMutex);
            m_generationQueueCV.wait(lock, [this] {
                return m_stopThreads || !m_generationQueue.empty();
            });

            if (m_stopThreads && m_generationQueue.empty()) return;

            if (!m_generationQueue.empty()) {
                task = m_generationQueue.front();
                m_generationQueue.pop();
            } else {
                continue;
            }
        }

        if (isChunkLoaded(task.chunkPos)) {
            task.chunk->setState(ChunkState::Generating);
            task.chunk->generate(&perlinNoise);

            if (isChunkLoaded(task.chunkPos)) {
                std::lock_guard<std::mutex> lock(m_meshBuildQueueMutex);
                m_meshBuildQueue.push({task.chunk, MeshData()});
                m_meshBuildQueueCV.notify_one();
            }
        }
    }
}

void World::meshBuildWorkerThread() {
    while (!m_stopThreads) {
        ChunkMeshTask task;

        {
            std::unique_lock<std::mutex> lock(m_meshBuildQueueMutex);
            m_meshBuildQueueCV.wait(lock, [this] {
                return m_stopThreads || !m_meshBuildQueue.empty();
            });

            if (m_stopThreads && m_meshBuildQueue.empty()) return;

            if (!m_meshBuildQueue.empty()) {
                task = m_meshBuildQueue.front();
                m_meshBuildQueue.pop();
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
            task.chunk->buildMeshData(task.meshData, &m_textureAtlas, this);

            if (isChunkLoaded(chunkPos)) {
                task.chunk->setState(ChunkState::MeshBuilt);
                {
                    std::lock_guard<std::mutex> lock(m_gpuUploadQueueMutex);
                    m_gpuUploadQueue.push(std::move(task));
                }
            }
        }
    }
}

void World::processGPUUploadQueue(int maxPerFrame) {
    PROFILE_SCOPE("World::processGPUUploadQueue");

    std::lock_guard<std::mutex> lock(m_gpuUploadQueueMutex);

    int processed = 0;
    while (!m_gpuUploadQueue.empty() && processed < maxPerFrame) {
        ChunkMeshTask& task = m_gpuUploadQueue.front();

        glm::ivec3 chunkPos = task.chunk->getPosition();

        if (isChunkLoaded(chunkPos) && task.chunk->getState() == ChunkState::MeshBuilt) {
            task.chunk->uploadMeshToGPU(task.meshData);
            // TODO: Implement proper neighbor remeshing with cycle detection
        }

        m_gpuUploadQueue.pop();
        processed++;
    }
}

bool World::loadTextureAtlas(const char* atlasPath, int tilesPerRow) {
    return m_textureAtlas.load(atlasPath, tilesPerRow);
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

        if (currentChunkPos != m_lastCameraChunkPos) {
            loadChunksAroundPosition(currentChunkPos);
            unloadDistantChunks(currentChunkPos);
            m_lastCameraChunkPos = currentChunkPos;
        }
    }

    {
        PROFILE_SCOPE("World::update - GPU uploads");
        processGPUUploadQueue(2);
    }
}

void World::render(Shader& shader) {
    m_textureAtlas.bind(0);

    for (auto& pair : m_chunks) {
        Chunk* chunk = pair.second.get();

        if (chunk->getState() != ChunkState::Ready) {
            continue;
        }

        glm::mat4 model = glm::mat4(1.0f);
        glm::vec3 worldPos = glm::vec3(pair.first) * static_cast<float>(Chunk::SIZE);
        model = glm::translate(model, worldPos);
        shader.setMat4("model", model);

        chunk->draw();
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
        std::lock_guard<std::mutex> lock(m_meshBuildQueueMutex);
        m_meshBuildQueue.push({chunk, MeshData()});
        m_meshBuildQueueCV.notify_one();
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
                Chunk* chunkPtr = chunk.get();
                m_chunks[chunkPos] = std::move(chunk);

                {
                    std::lock_guard<std::mutex> lock(m_generationQueueMutex);
                    m_generationQueue.push({chunkPos, chunkPtr});
                }
                m_generationQueueCV.notify_one();
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

        if (distance > m_renderDistance + 2) {
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