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

class Chunk {
public:
    static constexpr int SIZE = 64;
    static constexpr int HEIGHT = 196;

    Chunk(const glm::ivec3& position);
    ~Chunk() = default;

    BlockType getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockType type);

    void generate(const siv::PerlinNoise* perlinNoise = nullptr);

    // Split mesh generation into two phases
    void buildMeshData(MeshData& meshData, const TextureAtlas* atlas, World* world);

    void uploadMeshToGPU(const MeshData& meshData);

    void draw() const;

    glm::ivec3 getPosition() const { return m_position; }
    bool isDirty() const { return m_isDirty; }
    void markDirty() { m_isDirty = true; }

    ChunkState getState() const { return m_state.load(); }
    void setState(ChunkState state) { m_state.store(state); }

private:
    glm::ivec3 m_position;
    BlockType m_blocks[SIZE][HEIGHT][SIZE];
    Mesh m_mesh;
    bool m_isDirty;
    std::atomic<ChunkState> m_state;
    mutable std::mutex m_blocksMutex;

    bool isBlockAt(int x, int y, int z) const;
    bool shouldRenderFace(int x, int y, int z, int nx, int ny, int nz, World *world) const;

    void addFace(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices,
                 const glm::vec3& pos, int faceIndex, BlockType blockType,
                 const TextureAtlas* atlas);

};


#endif //GLFWVOXEL_CHUNK_H