//
// Created by maxim on 03/01/2026.
//

#ifndef GLFWVOXEL_CHUNK_H
#define GLFWVOXEL_CHUNK_H

#include <glm/glm.hpp>
#include "../rendering/Mesh.h"
#include "../rendering/TextureAltas.h"
#include "Block.h"

class TextureAtlas;
class World;

class Chunk {
public:
    static constexpr int SIZE = 16;
    static constexpr int HEIGHT = 256;

    Chunk(const glm::ivec3& position);
    ~Chunk() = default;

    BlockType getBlock(int x, int y, int z) const;
    void setBlock(int x, int y, int z, BlockType type);

    void generate();
    void generateMesh(const TextureAtlas* atlas = nullptr, const World* world = nullptr);
    void draw() const;

    glm::ivec3 getPosition() const { return m_position; }
    bool isDirty() const { return m_isDirty; }
    void markDirty() { m_isDirty = true; }

private:
    glm::ivec3 m_position;
    BlockType m_blocks[SIZE][HEIGHT][SIZE];
    Mesh m_mesh;
    bool m_isDirty;

    bool isBlockAt(int x, int y, int z) const;
    bool shouldRenderFace(int x, int y, int z, int nx, int ny, int nz, const World *world) const;

    void addFace(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices,
                 const glm::vec3& pos, int faceIndex, BlockType blockType,
                 const TextureAtlas* atlas);
};


#endif //GLFWVOXEL_CHUNK_H