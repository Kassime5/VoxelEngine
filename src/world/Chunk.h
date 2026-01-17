//
// Created by maxim on 03/01/2026.
//

#ifndef GLFWVOXEL_CHUNK_H
#define GLFWVOXEL_CHUNK_H

#include <glm/glm.hpp>
#include <atomic>
#include <mutex>
#include "../rendering/Mesh.h"
#include "../rendering/TextureAltas.h"
#include "Block.h"
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

struct QuadMask {
    BlockType blockType;
    int width;
    int height;
};

class Chunk {
public:
    static constexpr int SIZE = 64;
    static constexpr int HEIGHT = 256;

    Chunk(const glm::ivec3& position);
    ~Chunk() = default;

    void generate(const siv::PerlinNoise* perlinNoise = nullptr);
    void buildMeshData(MeshData& meshData, const TextureAtlas* atlas, World* world);
    void greedyMeshAxis(MeshData &meshData, const TextureAtlas *atlas, World *world, int axis);
    void uploadMeshToGPU(const MeshData& meshData);
    void draw() const;

    glm::ivec3 getPosition() const { return chunkPosition; }

    bool isDirty() const { return chunkDirty; }
    void markDirty() { chunkDirty = true; }

    ChunkState getState() const { return chunkState.load(); }
    void setState(ChunkState state) { chunkState.store(state); }

    BlockType getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockType type);
private:
    glm::ivec3 chunkPosition;
    BlockType chunkBlocks[SIZE][HEIGHT][SIZE];
    Mesh chunkMesh;
    bool chunkDirty;
    std::atomic<ChunkState> chunkState;

    bool isBlockAt(int x, int y, int z) const;
    bool shouldRenderFace(int x, int y, int z, int nx, int ny, int nz, World *world) const;

    void addFace(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices,
                 const glm::vec3& pos, int faceIndex, BlockType blockType,
                 const TextureAtlas* atlas);

    void addGreedyQuad(MeshData& meshData, int x[3], int du[3], int dv[3],
                          BlockType block, BlockFace face, const TextureAtlas *atlas,
                          int width, int height);
};


#endif //GLFWVOXEL_CHUNK_H