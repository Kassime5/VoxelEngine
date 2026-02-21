//
// Created by maxim on 15/02/2026.
//

#include "EntityMesh.h"

EntityMesh::EntityMesh() : VAO(0), VBO(0), EBO(0), vertexCount(0), indexCount(0), initialized(false) {}

EntityMesh::~EntityMesh() {
    cleanup();
}

EntityMesh::EntityMesh(EntityMesh&& other) noexcept
    : VAO(other.VAO), VBO(other.VBO), EBO(other.EBO),
      vertexCount(other.vertexCount), indexCount(other.indexCount),
      initialized(other.initialized) {
    other.VAO = 0;
    other.VBO = 0;
    other.EBO = 0;
    other.initialized = false;
}

EntityMesh& EntityMesh::operator=(EntityMesh&& other) noexcept {
    if (this != &other) {
        cleanup();

        VAO = other.VAO;
        VBO = other.VBO;
        EBO = other.EBO;
        vertexCount = other.vertexCount;
        indexCount = other.indexCount;
        initialized = other.initialized;

        other.VAO = 0;
        other.VBO = 0;
        other.EBO = 0;
        other.initialized = false;
    }
    return *this;
}

void EntityMesh::setData(const std::vector<SimpleVertex>& vertices,
                         const std::vector<unsigned int>& indices) {
    setupMesh(vertices, indices);
}

void EntityMesh::setupMesh(const std::vector<SimpleVertex>& vertices,
                           const std::vector<unsigned int>& indices) {
    if (vertices.empty()) return;

    if (initialized) cleanup();

    vertexCount = vertices.size();
    indexCount = indices.size();

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(SimpleVertex), vertices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    // Position attribute (location 0)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void*)offsetof(SimpleVertex, position));
    glEnableVertexAttribArray(0);

    // Normal attribute (location 1)
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(SimpleVertex), (void*)offsetof(SimpleVertex, normal));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
    initialized = true;
}

void EntityMesh::draw() const {
    if (!initialized || indexCount == 0) return;

    glBindVertexArray(VAO);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void EntityMesh::cleanup() {
    if (initialized) {
        glDeleteVertexArrays(1, &VAO);
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        VAO = VBO = EBO = 0;
        initialized = false;
    }
}

EntityMesh EntityMesh::createCube() {
    std::vector<SimpleVertex> vertices = {
        // Back face (z = -0.5)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
        // Front face (z = 0.5)
        {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
        // Left face (x = -0.5)
        {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}},
        // Right face (x = 0.5)
        {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
        // Bottom face (y = -0.5)
        {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}},
        // Top face (y = 0.5)
        {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}}
    };

    std::vector<unsigned int> indices = {
        // Back face (looking at it from behind, CCW)
        0, 3, 2,  2, 1, 0,
        // Front face (looking at it from front, CCW)
        4, 5, 6,  6, 7, 4,
        // Left face (looking at it from left, CCW)
        8, 11, 10,  10, 9, 8,
        // Right face (looking at it from right, CCW)
        12, 13, 14,  14, 15, 12,
        // Bottom face (looking at it from below, CCW)
        16, 17, 18,  18, 19, 16,
        // Top face (looking at it from above, CCW)
        20, 23, 22,  22, 21, 20
    };

    EntityMesh mesh;
    mesh.setData(vertices, indices);
    return mesh;
}