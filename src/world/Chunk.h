//
// Created by maxim on 03/01/2026.
//

#ifndef GLFWVOXEL_CHUNK_H
#define GLFWVOXEL_CHUNK_H

#include <glm/glm.hpp>
#include <atomic>
#include <mutex>
#include <optional>
#include "../rendering/Meshes/ChunkMesh.h"
#include "../rendering/TextureAltas.h"
#include "Block.h"
#include "Biome.h"
#include "WorleyBiome.h"
#include "PerlinNoise/PerlinNoise.hpp"

class TextureAtlas;
class World;

enum class ChunkState {
    Empty,           // Just created
    Generating,      // Terrain generation in progress
    Generated,       // Terrain ready, needs mesh
    BuildingMesh,    // Building mesh data (can be threaded)
    MeshBuilt,       // Mesh data ready, needs GPU upload
    Ready            // Mesh uploaded to GPU, ready to render
};

// Water is blended, so it meshes separately from everything else.
enum class MeshPass {
    Opaque,
    Water
};

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    void clear() {
        vertices.clear();
        indices.clear();
    }

    bool isEmpty() const {
        return vertices.empty();
    }
};

class Chunk {
public:
    static constexpr int SIZE = 64;
    static constexpr int HEIGHT = 256;

    // How far outside itself a chunk looks for structures whose footprint reaches in
    static constexpr int STRUCTURE_MARGIN = 4;

    // How far above SEA_LEVEL the sand band reaches before the biome's own surface takes over.
    static constexpr int BEACH_HEIGHT = 2;

    Chunk(const glm::ivec3& position);
    ~Chunk() = default;

    void generate(const siv::PerlinNoise* perlinNoise, WorleyBiome* worleyBiome);

    // Regular mesh
    void buildMeshData(MeshData& meshData, const TextureAtlas* atlas, World* world);
    void greedyMeshAxis(MeshData &meshData, const TextureAtlas *atlas, World *world, int axis,
                        MeshPass pass);
    void uploadMeshToGPU(const MeshData& meshData);
    void draw() const;

    // Transparent mesh
    void buildTransparentMeshData(MeshData& meshData, const TextureAtlas* atlas);
    void uploadTransparentMeshToGPU(const MeshData& meshData);
    void drawTransparent() const;
    bool isTransparentMeshEmpty() const { return chunkTransparentMesh.isEmpty(); }

    // Water mesh
    void buildWaterMeshData(MeshData& meshData, const TextureAtlas* atlas, World* world);
    void uploadWaterMeshToGPU(const MeshData& meshData);
    void drawWater() const;
    bool isWaterMeshEmpty() const { return chunkWaterMesh.isEmpty(); }

    glm::ivec3 getPosition() const { return chunkPosition; }

    bool isDirty() const { return chunkDirty; }
    void markDirty() { chunkDirty = true; }

    ChunkState getState() const { return chunkState.load(); }
    void setState(ChunkState state) { chunkState.store(state); }

    // A chunk only ever arrives once, so its neighbours only need invalidating once.
    // Main thread only, which is what stops the remesh cascade from looping.
    bool hasNotifiedNeighbours() const { return notifiedNeighbours; }
    void markNeighboursNotified() { notifiedNeighbours = true; }

    BlockType getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockType type);

    int getTerrainHeight(int localX, int localZ) const;
private:
    glm::ivec3 chunkPosition;
    BlockType chunkBlocks[SIZE][HEIGHT][SIZE];

    ChunkMesh chunkMesh;
    ChunkMesh chunkTransparentMesh;
    ChunkMesh chunkWaterMesh;

    bool hasWater = false;
    bool notifiedNeighbours = false;

    bool chunkDirty;
    std::atomic<ChunkState> chunkState;
    std::optional<glm::ivec3> structureSpawnPoint;

    void generateTerrain(const siv::PerlinNoise* perlinNoise, WorleyBiome* worleyBiome);
    void decorateTerrain(const siv::PerlinNoise* perlinNoise, WorleyBiome* worleyBiome);
    void placeStructures(const siv::PerlinNoise* perlinNoise, WorleyBiome* worleyBiome);

    bool isBlockAt(int x, int y, int z) const;
    bool shouldRenderFace(int x, int y, int z, int nx, int ny, int nz, World *world) const;

    void addFace(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices,
                 const glm::vec3& pos, int faceIndex, BlockType blockType,
                 const TextureAtlas* atlas);

    void addGreedyQuad(MeshData& meshData, int x[3], int du[3], int dv[3],
                          BlockType block, BlockFace face, const TextureAtlas *atlas,
                          int width, int height);

    void addCrossModel(MeshData& meshData, const glm::vec3& pos, BlockType blockType,
                  const TextureAtlas* atlas);
};


// Takes world coordinates and keeps only what lands inside one chunk. Structure code emits
// its whole shape through this and never learns where the chunk borders are.
class ChunkBlockSink {
public:
    explicit ChunkBlockSink(Chunk& chunk);

    void set(int worldX, int worldY, int worldZ, BlockType type);

private:
    Chunk& chunk;
    glm::ivec3 origin;
};

#endif //GLFWVOXEL_CHUNK_H