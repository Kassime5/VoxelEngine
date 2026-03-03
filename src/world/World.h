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
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include "src/debug/RenderStats.h"
#include "src/rendering/Camera.h"
#include "src/rendering/ShaderManager.h"
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

class World {
public:
    World(Player& _player);
    ~World();

    bool loadTextureAtlas(const char* atlasPath, int tilesPerRow = 16);
    void update(const glm::vec3& cameraPosition);

    void renderWorld(glm::mat4 projection, glm::mat4 view);

    BlockType getBlock(int worldX, int worldY, int worldZ);
    void setBlock(int worldX, int worldY, int worldZ, BlockType type);

    Chunk* getChunk(const glm::ivec3& chunkPos);
    Chunk* getChunkAt(int worldX, int worldY, int worldZ);

    void setRenderDistance(int distance) { renderDistance = distance; }
    int getRenderDistance() const { return renderDistance; }
    int getLoadedChunkCount() const { return m_chunks.size(); }
    const Biome* getCurrentPlayerBiome(float cameraX, float cameraZ) const;
    RaycastResult raycastBlock(const glm::vec3& origin, const glm::vec3& direction, float maxDistance = 10.0f);

    EntityManager* getEntityManager() { return &entityManager; }

private:
    Player& player;
    Shader* terrainShader;

    void render();
    void renderTransparent();

    std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, IVec3Hash> m_chunks;
    TextureAtlas textureAtlas;
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

    // Noise
    const siv::PerlinNoise::seed_type seed;
    const siv::PerlinNoise perlinNoise;
    std::unique_ptr<WorleyBiome> worleyBiome;

    glm::ivec3 worldToChunkPos(int worldX, int worldY, int worldZ) const;
    glm::ivec3 worldToLocalPos(int worldX, int worldY, int worldZ) const;

    void loadChunksAroundPosition(const glm::ivec3& centerChunkPos);
    void unloadDistantChunks(const glm::ivec3& centerChunkPos);
    bool isChunkLoaded(const glm::ivec3& chunkPos) const;

    EntityManager entityManager;
};

struct Frustum {
    glm::vec4 planes[6]; // left, right, bottom, top, near, far

    void extractFromMatrix(const glm::mat4& viewProj) {
        planes[0] = glm::vec4(
            viewProj[0][3] + viewProj[0][0],
            viewProj[1][3] + viewProj[1][0],
            viewProj[2][3] + viewProj[2][0],
            viewProj[3][3] + viewProj[3][0]
        );

        planes[1] = glm::vec4(
            viewProj[0][3] - viewProj[0][0],
            viewProj[1][3] - viewProj[1][0],
            viewProj[2][3] - viewProj[2][0],
            viewProj[3][3] - viewProj[3][0]
        );

        planes[2] = glm::vec4(
            viewProj[0][3] + viewProj[0][1],
            viewProj[1][3] + viewProj[1][1],
            viewProj[2][3] + viewProj[2][1],
            viewProj[3][3] + viewProj[3][1]
        );

        planes[3] = glm::vec4(
            viewProj[0][3] - viewProj[0][1],
            viewProj[1][3] - viewProj[1][1],
            viewProj[2][3] - viewProj[2][1],
            viewProj[3][3] - viewProj[3][1]
        );

        planes[4] = glm::vec4(
            viewProj[0][3] + viewProj[0][2],
            viewProj[1][3] + viewProj[1][2],
            viewProj[2][3] + viewProj[2][2],
            viewProj[3][3] + viewProj[3][2]
        );

        planes[5] = glm::vec4(
            viewProj[0][3] - viewProj[0][2],
            viewProj[1][3] - viewProj[1][2],
            viewProj[2][3] - viewProj[2][2],
            viewProj[3][3] - viewProj[3][2]
        );

        for (int i = 0; i < 6; i++) {
            float length = glm::length(glm::vec3(planes[i]));
            planes[i] /= length;
        }
    }

    bool isBoxInFrustum(const glm::vec3& min, const glm::vec3& max) const {
        for (int i = 0; i < 6; i++) {
            glm::vec3 positiveVertex(
                planes[i].x > 0 ? max.x : min.x,
                planes[i].y > 0 ? max.y : min.y,
                planes[i].z > 0 ? max.z : min.z
            );

            if (glm::dot(glm::vec3(planes[i]), positiveVertex) + planes[i].w < 0) {
                return false;
            }
        }
        return true;
    }
};

#endif //GLFWVOXEL_WORLD_H