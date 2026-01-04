//
// Created by maxim on 03/01/2026.
//

#ifndef GLFWVOXEL_MESH_H
#define GLFWVOXEL_MESH_H
#include <glad/glad.h>
#include <vector>
#include <glm/glm.hpp>

struct Vertex {
    glm::vec3 position;
    glm::vec2 texCoords;
    glm::vec3 normal;
};

class Mesh {
public:
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    Mesh();
    Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    ~Mesh();

    // Delete copy constructor/assignment to avoid double-free of OpenGL resources
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Move semantics for efficient transfers
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

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


#endif //GLFWVOXEL_MESH_H