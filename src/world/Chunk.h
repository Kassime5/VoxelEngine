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

#include <memory>

class TextureAtlas;
class World;
struct ChunkNeighbourhood;

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
    void buildMeshData(MeshData& meshData, const TextureAtlas* atlas, const ChunkNeighbourhood& neighbours);
    void greedyMeshAxis(MeshData &meshData, const TextureAtlas *atlas,
                        const ChunkNeighbourhood& neighbours, int axis, MeshPass pass);
    void uploadMeshToGPU(const MeshData& meshData);
    void draw() const;

    // Transparent mesh
    void buildTransparentMeshData(MeshData& meshData, const TextureAtlas* atlas);
    void uploadTransparentMeshToGPU(const MeshData& meshData);
    void drawTransparent() const;
    bool isTransparentMeshEmpty() const { return chunkTransparentMesh.isEmpty(); }

    // Water mesh
    void buildWaterMeshData(MeshData& meshData, const TextureAtlas* atlas, const ChunkNeighbourhood& neighbours);
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

    void addFace(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices,
                 const glm::vec3& pos, int faceIndex, BlockType blockType,
                 const TextureAtlas* atlas);

    void addGreedyQuad(MeshData& meshData, int x[3], int du[3], int dv[3],
                          BlockType block, BlockFace face, const TextureAtlas *atlas,
                          int width, int height);

    void addCrossModel(MeshData& meshData, const glm::vec3& pos, BlockType blockType,
                  const TextureAtlas* atlas);
};


// The four chunks a mesh build can read across its seams, pinned for the length of that
// build. Snapshotting them once keeps the map lock off the per-voxel path, and holding
// them by shared_ptr stops an unload from freeing a neighbour mid-build.
struct ChunkNeighbourhood {
    glm::ivec3 centre{0};
    std::shared_ptr<Chunk> negX, posX, negZ, posZ;

    // Only ever asked about blocks one step outside the centre chunk. Anything else reads as Air.
    BlockType getBlock(int worldX, int worldY, int worldZ) const {
        if (worldY < 0 || worldY >= Chunk::HEIGHT) {
            return BlockType::Air;
        }

        const int cx = worldX < 0 ? (worldX + 1) / Chunk::SIZE - 1 : worldX / Chunk::SIZE;
        const int cz = worldZ < 0 ? (worldZ + 1) / Chunk::SIZE - 1 : worldZ / Chunk::SIZE;
        const int dx = cx - centre.x;
        const int dz = cz - centre.z;

        const Chunk* chunk = nullptr;
        if (dx == -1 && dz == 0)      chunk = negX.get();
        else if (dx == 1 && dz == 0)  chunk = posX.get();
        else if (dx == 0 && dz == -1) chunk = negZ.get();
        else if (dx == 0 && dz == 1)  chunk = posZ.get();

        if (!chunk) {
            return BlockType::Air;
        }

        int localX = worldX % Chunk::SIZE;
        int localZ = worldZ % Chunk::SIZE;
        if (localX < 0) localX += Chunk::SIZE;
        if (localZ < 0) localZ += Chunk::SIZE;
        return chunk->getBlock(localX, worldY, localZ);
    }
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