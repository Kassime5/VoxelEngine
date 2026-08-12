//
// Created by maxim on 03/01/2026.
//

#ifndef GLFWVOXEL_CHUNKMESH_H
#define GLFWVOXEL_CHUNKMESH_H
#include <vector>
#include <glm/glm.hpp>

struct Vertex {
    glm::u8vec3 position;
    uint8_t tileIndex;
    uint8_t cornerIndex;
    uint8_t normalId;
    uint8_t quadWidth;
    uint8_t quadHeight;
};

class ChunkMesh {
public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    ChunkMesh();
    ChunkMesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    ~ChunkMesh();

    // Delete copy constructor/assignment to avoid double-free of OpenGL resources
    ChunkMesh(const ChunkMesh&) = delete;
    ChunkMesh& operator=(const ChunkMesh&) = delete;

    // Move semantics for efficient transfers
    ChunkMesh(ChunkMesh&& other) noexcept;
    ChunkMesh& operator=(ChunkMesh&& other) noexcept;

    void setData(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    void clear();
    void draw() const;

    bool isEmpty() const { return vertices.empty(); }

private:
    unsigned int VAO, VBO, EBO;
    bool initialized;

    void setupMesh();
    void cleanup();
};


#endif //GLFWVOXEL_CHUNKMESH_H